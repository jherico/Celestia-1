#include "VulkanRenderer.h"

#include <mutex>

#include <glm/gtc/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale, glm::perspective

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

#include <QtGui/QWindow>

#include <celengine/skygrid.h>

#include <vks/pipelines.hpp>
#include "CloseEventFilter.h"

#pragma optimize("", off)
using namespace Eigen;

static Vector3d toStandardCoords(const Vector3d& v) {
    return Vector3d(v.x(), -v.z(), v.y());
}

void VulkanRenderer::onWindowResized() {
    _resizing = true;
    _resizeTimer->start();
}

void VulkanRenderer::onResizeTimer() {
    waitIdle();
    _resizing = false;
    for (const auto& framebuffer : _framebuffers) {
        _device.destroy(framebuffer);
    }
    _framebuffers.clear();
    _swapchain.create(_extent, true);
    createFramebuffers();
}

void VulkanRenderer::onWindowClosing() {
    _ready = false;
    shutdown();
}

void VulkanRenderer::waitIdle() {
    _queue.waitIdle();
    _device.waitIdle();
}

// FIXME
static const std::string& getAssetPath() {
    static std::once_flag once;
    static std::string ASSET_PATH;
    std::call_once(once, [&] {
        auto dir = QFileInfo{ __FILE__ }.dir();
        dir.cd("../../resources");
        ASSET_PATH = QDir::cleanPath(dir.absolutePath()).toStdString() + "/";
    });
    return ASSET_PATH;
}

void VulkanRenderer::initialize() {
    _resizeTimer = new QTimer(this);
    _resizeTimer->setInterval(1000);
    _resizeTimer->setSingleShot(true);

    QObject::connect(_window, &QWindow::widthChanged, this, &VulkanRenderer::onWindowResized);
    QObject::connect(_window, &QWindow::heightChanged, this, &VulkanRenderer::onWindowResized);
    QObject::connect(_resizeTimer, &QTimer::timeout, this, &VulkanRenderer::onResizeTimer);

    {
        auto windowSize = _window->geometry().size();
        _extent = vk::Extent2D{ (uint32_t)windowSize.width(), (uint32_t)windowSize.height() };
    }

    auto closeEventFilter = new CloseEventFilter(_window);
    QObject::connect(closeEventFilter, &CloseEventFilter::closing, this, &VulkanRenderer::onWindowClosing);

    _context.enableValidation = true;
    _context.requireExtensions({ VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME });
    _context.requireDeviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });
    _context.createInstance(VK_MAKE_VERSION(1, 0, 0));
    auto surface = _context.instance.createWin32SurfaceKHR(vk::Win32SurfaceCreateInfoKHR{ {}, GetModuleHandle(NULL), (HWND)_window->winId() });
    _context.createDevice(surface);

    _semaphores.acquireComplete = _device.createSemaphore({});
    _semaphores.renderComplete = _device.createSemaphore({});

    _swapchain.setup(_context.physicalDevice, _context.device, _context.queue, _context.queueIndices.graphics);
    _swapchain.setSurface(surface);
    _swapchain.create(_extent, true);
    createRenderPass();
    createFramebuffers();
    createDescriptorPool();

    // Camera setup
    {
        _camera.ubo = _context.createUniformBuffer(_camera.matrices);
        _camera.ubo.setupDescriptor();

        std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings{
            { 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment },
        };
        _camera.descriptorSetLayout = _device.createDescriptorSetLayout({ {}, (uint32_t)setLayoutBindings.size(), setLayoutBindings.data() });
        _camera.descriptorSet = _device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{ _descriptorPool, 1, &_camera.descriptorSetLayout })[0];
        std::vector<vk::WriteDescriptorSet> writeDescriptorSets{
            { _camera.descriptorSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &_camera.ubo.descriptor },
        };
        _device.updateDescriptorSets(writeDescriptorSets, nullptr);
    }

    // skygrid setup
    {
        // Vertices
        {
            static const uint32_t MERIDIANS = 24;
            static const uint32_t GREAT_CIRCLE_SEGMENTS = 128;
            static const float phiInterval = TAUf / MERIDIANS;
            static const float thetaInterval = HALF_TAUf / GREAT_CIRCLE_SEGMENTS;

            std::vector<glm::vec3> vertices;
            vertices.reserve(MERIDIANS * GREAT_CIRCLE_SEGMENTS);
            std::vector<uint32_t> indices;
            indices.reserve(MERIDIANS * GREAT_CIRCLE_SEGMENTS);
            for (uint32_t i = 0; i < MERIDIANS; ++i) {
                float phi = (phiInterval * i) - HALF_TAUf;
                float sinPhi = sin(phi);
                float cosPhi = cos(phi);
                for (uint32_t j = 0; j < GREAT_CIRCLE_SEGMENTS; ++j) {
                    float theta = (j * thetaInterval);
                    float sinTheta = sin(theta);
                    float cosTheta = cos(theta);
                    glm::vec3 vertex{ sinTheta * cosPhi, sinTheta * sinPhi, cosTheta };
                    vertex *= 0.5f;
                    indices.push_back((uint32_t)vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(UINT32_MAX);
            }
            _skyGrids.vertices = _context.stageToDeviceBuffer<glm::vec3>(vk::BufferUsageFlagBits::eVertexBuffer, vertices);
            _skyGrids.indices = _context.stageToDeviceBuffer<uint32_t>(vk::BufferUsageFlagBits::eIndexBuffer, indices);
            _skyGrids.indexCount = indices.size();
        }

        // Pipeline layout
        {
            vk::PushConstantRange pushConstantRange{ vk::ShaderStageFlagBits::eVertex, 0, sizeof(SkyGrids::PushContstants) };
            _skyGrids.pipelineLayout = _device.createPipelineLayout(vk::PipelineLayoutCreateInfo{ {}, 1, &_camera.descriptorSetLayout, 1, &pushConstantRange });
        }

        {
            vks::pipelines::GraphicsPipelineBuilder builder{ _device, _context.pipelineCache };
            builder.layout = _skyGrids.pipelineLayout;
            builder.renderPass = _renderPass;
            builder.inputAssemblyState.primitiveRestartEnable = VK_TRUE;
            builder.inputAssemblyState.topology = vk::PrimitiveTopology::eLineStrip;
            builder.vertexInputState.bindingDescriptions = { { 0, sizeof(glm::vec3), vk::VertexInputRate::eVertex } };
            builder.vertexInputState.attributeDescriptions = { { 0, 0, vk::Format::eR32G32B32Sfloat } };
            builder.dynamicState.dynamicStateEnables = {
                vk::DynamicState::eViewport,
                vk::DynamicState::eScissor,
                vk::DynamicState::eLineWidth,
            };
            builder.loadShader(getAssetPath() + "shaders/skygrid.vert.spv", vk::ShaderStageFlagBits::eVertex);
            builder.loadShader(getAssetPath() + "shaders/skygrid.frag.spv", vk::ShaderStageFlagBits::eFragment);
            _skyGrids.pipeline = builder.create();
        }
    }

    _ready = true;
}

void VulkanRenderer::createRenderPass() {
    std::vector<vk::AttachmentDescription> attachments;
    std::vector<vk::AttachmentReference> attachmentReferences;
    std::vector<vk::SubpassDescription> subpasses;
    std::vector<vk::SubpassDependency> subpassDependencies;

    vk::AttachmentDescription colorAttachment;
    colorAttachment.format = _swapchain.colorFormat;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;
    attachments.push_back(colorAttachment);

    vk::AttachmentReference colorAttachmentReference;
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = vk::ImageLayout::eColorAttachmentOptimal;
    attachmentReferences.push_back(colorAttachmentReference);

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = attachmentReferences.data();
    subpasses.push_back(subpass);

    using vAF = vk::AccessFlagBits;
    using vPSF = vk::PipelineStageFlagBits;
    subpassDependencies.push_back({ 0u, VK_SUBPASS_EXTERNAL, vPSF::eColorAttachmentOutput, vPSF::eBottomOfPipe, vAF::eColorAttachmentWrite,
                                    vAF::eColorAttachmentRead, vk::DependencyFlagBits::eByRegion });
    _renderPass = _device.createRenderPass({ {},
                                             (uint32_t)attachments.size(),
                                             attachments.data(),
                                             (uint32_t)subpasses.size(),
                                             subpasses.data(),
                                             (uint32_t)subpassDependencies.size(),
                                             subpassDependencies.data() });
}

void VulkanRenderer::createFramebuffers() {
    std::array<vk::ImageView, 1> imageViews;
    vk::FramebufferCreateInfo framebufferCreateInfo;
    framebufferCreateInfo.renderPass = _renderPass;
    // Create a placeholder image view for the swap chain color attachments
    framebufferCreateInfo.attachmentCount = (uint32_t)imageViews.size();
    framebufferCreateInfo.pAttachments = imageViews.data();
    framebufferCreateInfo.width = _extent.width;
    framebufferCreateInfo.height = _extent.height;
    framebufferCreateInfo.layers = 1;
    // Create frame buffers for every swap chain image
    _framebuffers = _swapchain.createFramebuffers(framebufferCreateInfo);
}

void VulkanRenderer::createDescriptorPool() {
    // Descriptor Pool
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        { vk::DescriptorType::eUniformBuffer, 4 },
    };
    _descriptorPool = _device.createDescriptorPool({ {}, 2, (uint32_t)poolSizes.size(), poolSizes.data() });
}

void VulkanRenderer::shutdown() {
    if (_context.instance) {
        _ready = false;
        waitIdle();
        for (const auto& framebuffer : _framebuffers) {
            _device.destroy(framebuffer);
        }
        _framebuffers.clear();
        if (_frame.commandBuffer) {
            _device.freeCommandBuffers(_context.getCommandPool(), _frame.commandBuffer);
        }
        _device.destroySemaphore(_semaphores.acquireComplete);
        _device.destroySemaphore(_semaphores.renderComplete);
        _swapchain.destroy();
        _context.destroy();
    }
}

static const uint32_t LOOP_INTERVAL_MS = 10000;

void VulkanRenderer::render(const ObserverPtr& observerPtr, const UniversePtr& universePtr, float faintestVisible, const Selection& sel) {
    if (_resizing || !_ready) {
        return;
    }
    const auto& observer = *observerPtr;
    const auto& universe = *universePtr;
    preRender(observer, universe, faintestVisible, sel);

    _camera.matrices.projection = glm::perspective(TAUf / 8.0f, 4.0f / 3.0f, 0.01f, 100.f);
    auto msecs = QDateTime::currentMSecsSinceEpoch() % LOOP_INTERVAL_MS;
    float interval = (float)msecs / (float)LOOP_INTERVAL_MS;
    
    _camera.matrices.view = glm::rotate(glm::mat4(), (float)interval * TAUf, glm::vec3{ 1, 0, 0 });
    _camera.ubo.copy(_camera.matrices);

    uint32_t currentBuffer = _swapchain.acquireNextImage(_semaphores.acquireComplete).value;

    if (_frame.commandBuffer) {
        _context.trash(_frame.commandBuffer);
    }

    static const vk::ClearValue CLEAR_COLOR{ vks::util::clearColor({ 0, 0, 0, 1 }) };
    _frame.commandBuffer = _context.allocateCommandBuffers(1)[0];
    _frame.framebuffer = _framebuffers[currentBuffer];
    _frame.commandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    _frame.commandBuffer.beginRenderPass({ _renderPass, _frame.framebuffer, { {}, _extent }, 1, &CLEAR_COLOR }, vk::SubpassContents::eInline);



    _frame.commandBuffer.setViewport(0, vks::util::viewport(_extent));
    _frame.commandBuffer.setScissor(0, vks::util::rect2D(_extent));

    // Render sky grids first--these will always be in the background
    renderSkyGrids(observer);

    // Render deep sky objects
    if ((renderFlags & (ShowGalaxies | ShowGlobulars | ShowNebulae | ShowOpenClusters)) != 0 && universe.getDSOCatalog()) {
        renderDeepSkyObjects(universe, observer, faintestMag);
    }

    // Render stars
    if ((renderFlags & ShowStars) != 0 && universe.getStarCatalog() != NULL) {
        renderStars(*universe.getStarCatalog(), faintestMag, observer);
    }

    // Render asterisms
    if ((renderFlags & ShowDiagrams) != 0 && !universe.getAsterisms().empty()) {
        /* We'll linearly fade the lines as a function of the observer's distance to the origin of coordinates: */
        //float opacity = 1.0f;
        //float dist = observerPosLY.norm() * 1.6e4f;
        //if (dist > MaxAsterismLinesConstDist) {
        //    opacity = clamp((MaxAsterismLinesConstDist - dist) / (MaxAsterismLinesDist - MaxAsterismLinesConstDist) + 1);
        //}

        for (const auto& ast : universe.getAsterisms()) {
            if (ast->getActive()) {
                if (ast->isColorOverridden()) {
                } else {
                }

                for (int i = 0; i < ast->getChainCount(); i++) {
                    const Asterism::Chain& chain = ast->getChain(i);
                    //glBegin(GL_LINE_STRIP);
                    //for (Asterism::Chain::const_iterator iter = chain.begin(); iter != chain.end(); iter++)
                    //    glVertex3fv(iter->data());
                    //glEnd();
                }
            }
        }
    }

    if ((renderFlags & ShowBoundaries) != 0) {
        /* We'll linearly fade the boundaries as a function of the observer's distance to the origin of coordinates: */
        //float opacity = 1.0f;
        //float dist = observerPosLY.norm() * 1.6e4f;
        //if (dist > MaxAsterismLabelsConstDist) {
        //    opacity = clamp((MaxAsterismLabelsConstDist - dist) / (MaxAsterismLabelsDist - MaxAsterismLabelsConstDist) + 1);
        //}
        //if (universe.getBoundaries() != NULL)
        //    universe.getBoundaries()->render();
    }

    // Render star and deep sky object labels
    //renderBackgroundAnnotations(FontNormal);
    // Render constellations labels
    //if ((labelMode & ConstellationLabels) != 0 && universe.getAsterisms() != NULL) {
    //    labelConstellations(*universe.getAsterisms(), observer);
    //    renderBackgroundAnnotations(FontLarge);
    //}
    _frame.commandBuffer.endRenderPass();
    _frame.commandBuffer.end();

    auto fence = _device.createFence({});
    _context.submit(_frame.commandBuffer, { _semaphores.acquireComplete, vk::PipelineStageFlagBits::eBottomOfPipe }, _semaphores.renderComplete, fence);
    _swapchain.queuePresent(_semaphores.renderComplete);
    _context.emptyDumpster(fence);
    _device.waitForFences(fence, VK_TRUE, UINT64_MAX);
    _context.recycle();
}

template <typename PREC>
glm::tquat<PREC, glm::highp> toGlm(const Eigen::Quaternion<PREC>& q) {
    return glm::tquat<PREC, glm::highp>(q.w(), q.x(), q.y(), q.z());
}

template <typename PREC>
glm::mat4 toMat4(const Eigen::Quaternion<PREC>& q) {
    return glm::mat4_cast(glm::quat(toGlm<double>(q)));
}

glm::vec4 toGlm(const Color& color) {
    return glm::vec4(color.red(), color.green(), color.blue(), color.alpha());
}



void VulkanRenderer::renderSkyGrids(const Observer& observer) {
    static const auto ALL_GRIDS = ShowCelestialSphere | ShowGalacticGrid | ShowEclipticGrid;

    // DEBUG
    renderFlags |= ALL_GRIDS;

    if (0 == (renderFlags & ALL_GRIDS)) {
        return;
    }


    // Submit via push constant (rather than a UBO)
    _frame.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _skyGrids.pipeline);
    _frame.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _skyGrids.pipelineLayout, 0, _camera.descriptorSet, nullptr);
    _frame.commandBuffer.bindVertexBuffers(0, _skyGrids.vertices.buffer, { 0 });
    _frame.commandBuffer.bindIndexBuffer(_skyGrids.indices.buffer, 0, vk::IndexType::eUint32);
    _frame.commandBuffer.setLineWidth(1.5f);



    if (renderFlags & ShowCelestialSphere) {
        _skyGrids.pushConstants.orientation = toMat4(Quaterniond(AngleAxis<double>(astro::J2000Obliquity, Vector3d::UnitX())));
        _skyGrids.pushConstants.color = toGlm(EquatorialGridColor);
        _frame.commandBuffer.pushConstants<SkyGrids::PushContstants>(_skyGrids.pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, _skyGrids.pushConstants);
        _frame.commandBuffer.drawIndexed(_skyGrids.indexCount, 1, 0, 0, 0);
    }

    if (renderFlags & ShowGalacticGrid) {
        _skyGrids.pushConstants.orientation = toMat4((astro::eclipticToEquatorial() * astro::equatorialToGalactic()).conjugate());
        _skyGrids.pushConstants.color = toGlm(GalacticGridColor);
        _frame.commandBuffer.pushConstants<SkyGrids::PushContstants>(_skyGrids.pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, _skyGrids.pushConstants);
        _frame.commandBuffer.drawIndexed(_skyGrids.indexCount, 1, 0, 0, 0);
    }

    if (renderFlags & ShowEclipticGrid) {
        _skyGrids.pushConstants.orientation = glm::mat4();
        _skyGrids.pushConstants.color = toGlm(EclipticGridColor);
        _frame.commandBuffer.pushConstants<SkyGrids::PushContstants>(_skyGrids.pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, _skyGrids.pushConstants);
        _frame.commandBuffer.drawIndexed(_skyGrids.indexCount, 1, 0, 0, 0);
    }

    if (renderFlags & ShowHorizonGrid) {
        double tdb = observer.getTime();
        const auto& frame = observer.getFrame();
        const auto body = frame->getRefObject().body();
        if (body) {
            SkyGrid grid;
            grid.setLineColor(HorizonGridColor);
            grid.setLabelColor(HorizonGridLabelColor);
            grid.setLongitudeUnits(SkyGrid::LongitudeDegrees);
            grid.setLongitudeDirection(SkyGrid::IncreasingClockwise);

            Vector3d zenithDirection = observer.getPosition().offsetFromKm(body->getPosition(tdb)).normalized();

            Vector3d northPole = body->getEclipticToEquatorial(tdb).conjugate() * Vector3d::UnitY();
            zenithDirection = toStandardCoords(zenithDirection);
            northPole = toStandardCoords(northPole);

            Vector3d v = zenithDirection.cross(northPole);

            // Horizontal coordinate system not well defined when observer
            // is at a pole.
            double tolerance = 1.0e-10;
            if (v.norm() > tolerance && v.norm() < 1.0 - tolerance) {
                v.normalize();
                Vector3d u = v.cross(zenithDirection);

                Matrix3d m;
                m.row(0) = u;
                m.row(1) = v;
                m.row(2) = zenithDirection;
                grid.setOrientation(Quaterniond(m));

                grid.render(*this, observer);
            }
        }
    }

    if (renderFlags & ShowEcliptic) {
        // Draw the J2000.0 ecliptic; trivial, since this forms the basis for
        // Celestia's coordinate system.
        //const int subdivision = 200;
        //glColor(EclipticColor);
        //glBegin(GL_LINE_LOOP);
        //for (int i = 0; i < subdivision; i++) {
        //    double theta = (double)i / (double)subdivision * 2 * PI;
        //    glVertex3f((float)cos(theta) * 1000.0f, 0.0f, (float)sin(theta) * 1000.0f);
        //}
        //glEnd();
    }
}

void VulkanRenderer::renderDeepSkyObjects(const Universe& universe, const Observer& observer, const float faintestMagNight) {
}

void VulkanRenderer::renderStars(const StarDatabase& starDB, float faintestMagNight, const Observer& observer) {
}

#if 0
void foo() {
    // Render sky grids first--these will always be in the background
    {
        glDisable(GL_TEXTURE_2D);
        if ((renderFlags & ShowSmoothLines) != 0)
            enableSmoothLines();
        renderSkyGrids(observer);
        if ((renderFlags & ShowSmoothLines) != 0)
            disableSmoothLines();
        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
    }

    // Render deep sky objects
    if ((renderFlags & (ShowGalaxies | ShowGlobulars | ShowNebulae | ShowOpenClusters)) != 0 &&
        universe.getDSOCatalog() != NULL) {
        renderDeepSkyObjects(universe, observer, faintestMag);
    }

    // Translate the camera before rendering the stars
    glPushMatrix();

// Render stars
#ifdef USE_HDR
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
#endif
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    if ((renderFlags & ShowStars) != 0 && universe.getStarCatalog() != NULL) {
        // Disable multisample rendering when drawing point stars
        bool toggleAA = (starStyle == Renderer::PointStars && glIsEnabled(GL_MULTISAMPLE_ARB));
        if (toggleAA)
            glDisable(GL_MULTISAMPLE_ARB);

        if (useNewStarRendering)
            renderPointStars(*universe.getStarCatalog(), faintestMag, observer);
        else
            renderStars(*universe.getStarCatalog(), faintestMag, observer);

        if (toggleAA)
            glEnable(GL_MULTISAMPLE_ARB);
    }

#ifdef USE_HDR
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif

    glTranslatef(-observerPosLY.x(), -observerPosLY.y(), -observerPosLY.z());

    // Render asterisms
    if ((renderFlags & ShowDiagrams) != 0 && universe.getAsterisms() != NULL) {
        /* We'll linearly fade the lines as a function of the observer's
    distance to the origin of coordinates: */
        float opacity = 1.0f;
        float dist = observerPosLY.norm() * 1.6e4f;
        if (dist > MaxAsterismLinesConstDist) {
            opacity = clamp((MaxAsterismLinesConstDist - dist) / (MaxAsterismLinesDist - MaxAsterismLinesConstDist) + 1);
        }

        glColor(ConstellationColor, opacity);
        glDisable(GL_TEXTURE_2D);
        if ((renderFlags & ShowSmoothLines) != 0)
            enableSmoothLines();
        AsterismList* asterisms = universe.getAsterisms();
        for (AsterismList::const_iterator iter = asterisms->begin(); iter != asterisms->end(); iter++) {
            Asterism* ast = *iter;

            if (ast->getActive()) {
                if (ast->isColorOverridden())
                    glColor(ast->getOverrideColor(), opacity);
                else
                    glColor(ConstellationColor, opacity);

                for (int i = 0; i < ast->getChainCount(); i++) {
                    const Asterism::Chain& chain = ast->getChain(i);

                    glBegin(GL_LINE_STRIP);
                    for (Asterism::Chain::const_iterator iter = chain.begin(); iter != chain.end(); iter++)
                        glVertex3fv(iter->data());
                    glEnd();
                }
            }
        }

        if ((renderFlags & ShowSmoothLines) != 0)
            disableSmoothLines();
    }

    if ((renderFlags & ShowBoundaries) != 0) {
        /* We'll linearly fade the boundaries as a function of the
    observer's distance to the origin of coordinates: */
        float opacity = 1.0f;
        float dist = observerPosLY.norm() * 1.6e4f;
        if (dist > MaxAsterismLabelsConstDist) {
            opacity = clamp((MaxAsterismLabelsConstDist - dist) / (MaxAsterismLabelsDist - MaxAsterismLabelsConstDist) + 1);
        }
        glColor(BoundaryColor, opacity);

        glDisable(GL_TEXTURE_2D);
        if ((renderFlags & ShowSmoothLines) != 0)
            enableSmoothLines();
        if (universe.getBoundaries() != NULL)
            universe.getBoundaries()->render();
        if ((renderFlags & ShowSmoothLines) != 0)
            disableSmoothLines();
    }

    // Render star and deep sky object labels
    renderBackgroundAnnotations(FontNormal);

    // Render constellations labels
    if ((labelMode & ConstellationLabels) != 0 && universe.getAsterisms() != NULL) {
        labelConstellations(*universe.getAsterisms(), observer);
        renderBackgroundAnnotations(FontLarge);
    }

    // Pop observer translation
    glPopMatrix();

    if ((renderFlags & ShowMarkers) != 0) {
        renderMarkers(*universe.getMarkers(), observer.getPosition(), observer.getOrientation(), now);

        // Render background markers; rendering of other markers is deferred until
        // solar system objects are rendered.
        renderBackgroundAnnotations(FontNormal);
    }

    // Draw the selection cursor
    bool selectionVisible = false;
    if (!sel.empty() && (renderFlags & ShowMarkers)) {
        Vector3d offset = sel.getPosition(now).offsetFromKm(observer.getPosition());

        static MarkerRepresentation cursorRep(MarkerRepresentation::Crosshair);
        selectionVisible = xfrustum.testSphere(offset, sel.radius()) != Frustum::Outside;

        if (selectionVisible) {
            double distance = offset.norm();
            float symbolSize = (float)(sel.radius() / distance) / pixelSize;

            // Modify the marker position so that it is always in front of the marked object.
            double boundingRadius;
            if (sel.body() != NULL)
                boundingRadius = sel.body()->getBoundingRadius();
            else
                boundingRadius = sel.radius();
            offset *= (1.0 - boundingRadius * 1.01 / distance);

            // The selection cursor is only partially visible when the selected object is obscured. To implement
            // this behavior we'll draw two markers at the same position: one that's always visible, and another one
            // that's depth sorted. When the selection is occluded, only the foreground marker is visible. Otherwise,
            // both markers are drawn and cursor appears much brighter as a result.
            if (distance < astro::lightYearsToKilometers(1.0)) {
                addSortedAnnotation(&cursorRep, EMPTY_STRING, Color(SelectionCursorColor, 1.0f), offset.cast<float>(),
                                    AlignLeft, VerticalAlignTop, symbolSize);
            } else {
                addAnnotation(backgroundAnnotations, &cursorRep, EMPTY_STRING, Color(SelectionCursorColor, 1.0f),
                              offset.cast<float>(), AlignLeft, VerticalAlignTop, symbolSize);
            }

            Color occludedCursorColor(SelectionCursorColor.red(), SelectionCursorColor.green() + 0.3f,
                                      SelectionCursorColor.blue());
            addAnnotation(foregroundAnnotations, &cursorRep, EMPTY_STRING, Color(occludedCursorColor, 0.4f),
                          offset.cast<float>(), AlignLeft, VerticalAlignTop, symbolSize);
        }
    }

    glPolygonMode(GL_FRONT, (GLenum)renderMode);
    glPolygonMode(GL_BACK, (GLenum)renderMode);

    {
        Matrix3f viewMat = observer.getOrientationf().conjugate().toRotationMatrix();

        // Remove objects from the render list that lie completely outside the
        // view frustum.
#ifdef USE_HDR
        maxBodyMag = maxBodyMagPrev;
        float starMaxMag = maxBodyMagPrev;
        notCulled = renderList.begin();
#else
        vector<RenderListEntry>::iterator notCulled = renderList.begin();
#endif
        for (vector<RenderListEntry>::iterator iter = renderList.begin(); iter != renderList.end(); iter++) {
#ifdef USE_HDR
            switch (iter->renderableType) {
                case RenderListEntry::RenderableStar:
                    break;
                default:
                    *notCulled = *iter;
                    notCulled++;
                    continue;
            }
#endif
            Vector3f center = viewMat.transpose() * iter->position;

            bool convex = true;
            float radius = 1.0f;
            float cullRadius = 1.0f;
            float cloudHeight = 0.0f;

#ifndef USE_HDR
            switch (iter->renderableType) {
                case RenderListEntry::RenderableStar:
                    radius = iter->star->getRadius();
                    cullRadius = radius * (1.0f + CoronaHeight);
                    break;

                case RenderListEntry::RenderableCometTail:
                    radius = iter->radius;
                    cullRadius = radius;
                    convex = false;
                    break;

                case RenderListEntry::RenderableBody:
                    radius = iter->body->getBoundingRadius();
                    if (iter->body->getRings() != NULL) {
                        radius = iter->body->getRings()->outerRadius;
                        convex = false;
                    }

                    if (!iter->body->isEllipsoid())
                        convex = false;

                    cullRadius = radius;
                    if (iter->body->getAtmosphere() != NULL) {
                        cullRadius += iter->body->getAtmosphere()->height;
                        cloudHeight =
                            max(iter->body->getAtmosphere()->cloudHeight,
                                iter->body->getAtmosphere()->mieScaleHeight * (float)-log(AtmosphereExtinctionThreshold));
                    }
                    break;

                case RenderListEntry::RenderableReferenceMark:
                    radius = iter->radius;
                    cullRadius = radius;
                    convex = false;
                    break;

                default:
                    break;
            }
#else
            radius = iter->star->getRadius();
            cullRadius = radius * (1.0f + CoronaHeight);
#endif  // USE_HDR

            // Test the object's bounding sphere against the view frustum
            if (frustum.testSphere(center, cullRadius) != Frustum::Outside) {
                float nearZ = center.norm() - radius;
#ifdef USE_HDR
                nearZ = -nearZ * nearZcoeff;
#else
                float maxSpan = (float)sqrt(square((float)windowWidth) + square((float)windowHeight));

                nearZ = -nearZ * (float)cos(degToRad(fov / 2)) * ((float)windowHeight / maxSpan);
#endif
                if (nearZ > -MinNearPlaneDistance)
                    iter->nearZ = -max(MinNearPlaneDistance, radius / 2000.0f);
                else
                    iter->nearZ = nearZ;

                if (!convex) {
                    iter->farZ = center.z() - radius;
                    if (iter->farZ / iter->nearZ > MaxFarNearRatio * 0.5f)
                        iter->nearZ = iter->farZ / (MaxFarNearRatio * 0.5f);
                } else {
                    // Make the far plane as close as possible
                    float d = center.norm();

                    // Account for ellipsoidal objects
                    float eradius = radius;
                    if (iter->renderableType == RenderListEntry::RenderableBody) {
                        float minSemiAxis = iter->body->getSemiAxes().minCoeff();
                        eradius *= minSemiAxis / radius;
                    }

                    if (d > eradius) {
                        iter->farZ = iter->centerZ - iter->radius;
                    } else {
                        // We're inside the bounding sphere (and, if the planet
                        // is spherical, inside the planet.)
                        iter->farZ = iter->nearZ * 2.0f;
                    }

                    if (cloudHeight > 0.0f) {
                        // If there's a cloud layer, we need to move the
                        // far plane out so that the clouds aren't clipped
                        float cloudLayerRadius = eradius + cloudHeight;
                        iter->farZ -= (float)sqrt(square(cloudLayerRadius) - square(eradius));
                    }
                }

                *notCulled = *iter;
                notCulled++;
#ifdef USE_HDR
                if (iter->discSizeInPixels > 1.0f && iter->appMag < starMaxMag) {
                    starMaxMag = iter->appMag;
                    brightestStar = iter->star;
                    foundBrightestStar = true;
                }
#endif
            }
        }

        renderList.resize(notCulled - renderList.begin());

        // The calls to buildRenderLists/renderStars filled renderList
        // with visible bodies.  Sort it front to back, then
        // render each entry in reverse order (TODO: convenient, but not
        // ideal for performance; should render opaque objects front to
        // back, then translucent objects back to front. However, the
        // amount of overdraw in Celestia is typically low.)
        sort(renderList.begin(), renderList.end());

        // Sort the annotations
        sort(depthSortedAnnotations.begin(), depthSortedAnnotations.end());

        // Sort the orbit paths
        sort(orbitPathList.begin(), orbitPathList.end());

        int nEntries = renderList.size();

#ifdef USE_HDR
        // Compute 1 eclipse between eye - closest body - brightest star
        // This prevents an eclipsed star from increasing exposure
        bool eyeNotEclipsed = true;
        closestBody = renderList.empty() ? renderList.end() : renderList.begin();
        if (foundClosestBody && closestBody != renderList.end() &&
            closestBody->renderableType == RenderListEntry::RenderableBody && closestBody->body && brightestStar) {
            const Body* body = closestBody->body;
            double scale = astro::microLightYearsToKilometers(1.0);
            Point3d posBody = body->getAstrocentricPosition(now);
            Point3d posStar;
            Point3d posEye = astrocentricPosition(observer.getPosition(), *brightestStar, now);

            if (body->getSystem() && body->getSystem()->getStar() && body->getSystem()->getStar() != brightestStar) {
                UniversalCoord center = body->getSystem()->getStar()->getPosition(now);
                Vec3d v = brightestStar->getPosition(now) - center;
                posStar = Point3d(v.x, v.y, v.z);
            } else {
                posStar = brightestStar->getPosition(now);
            }

            posStar.x /= scale;
            posStar.y /= scale;
            posStar.z /= scale;
            Vec3d lightToBodyDir = posBody - posStar;
            Vec3d bodyToEyeDir = posEye - posBody;

            if (lightToBodyDir * bodyToEyeDir > 0.0) {
                double dist = distance(posEye, Ray3d(posBody, lightToBodyDir));
                if (dist < body->getRadius())
                    eyeNotEclipsed = false;
            }
        }

        if (eyeNotEclipsed) {
            maxBodyMag = min(maxBodyMag, starMaxMag);
        }
#endif

        // Since we're rendering objects of a huge range of sizes spread over
        // vast distances, we can't just rely on the hardware depth buffer to
        // handle hidden surface removal without a little help. We'll partition
        // the depth buffer into spans that can be rendered without running
        // into terrible depth buffer precision problems. Typically, each body
        // with an apparent size greater than one pixel is allocated its own
        // depth buffer interval. However, this will not correctly handle
        // overlapping objects.  If two objects overlap in depth, we must
        // assign them to the same interval.

        depthPartitions.clear();
        int nIntervals = 0;
        float prevNear = -1e12f;  // ~ 1 light year
        if (nEntries > 0)
            prevNear = renderList[nEntries - 1].farZ * 1.01f;

        int i;

        // Completely partition the depth buffer. Scan from back to front
        // through all the renderable items that passed the culling test.
        for (i = nEntries - 1; i >= 0; i--) {
            // Only consider renderables that will occupy more than one pixel.
            if (renderList[i].discSizeInPixels > 1) {
                if (nIntervals == 0 || renderList[i].farZ >= depthPartitions[nIntervals - 1].nearZ) {
                    // This object spans a depth interval that's disjoint with
                    // the current interval, so create a new one for it, and
                    // another interval to fill the gap between the last
                    // interval.
                    DepthBufferPartition partition;
                    partition.index = nIntervals;
                    partition.nearZ = renderList[i].farZ;
                    partition.farZ = prevNear;

                    // Omit null intervals
                    // TODO: Is this necessary? Shouldn't the >= test prevent this?
                    if (partition.nearZ != partition.farZ) {
                        depthPartitions.push_back(partition);
                        nIntervals++;
                    }

                    partition.index = nIntervals;
                    partition.nearZ = renderList[i].nearZ;
                    partition.farZ = renderList[i].farZ;
                    depthPartitions.push_back(partition);
                    nIntervals++;

                    prevNear = partition.nearZ;
                } else {
                    // This object overlaps the current span; expand the
                    // interval so that it completely contains the object.
                    DepthBufferPartition& partition = depthPartitions[nIntervals - 1];
                    partition.nearZ = max(partition.nearZ, renderList[i].nearZ);
                    partition.farZ = min(partition.farZ, renderList[i].farZ);
                    prevNear = partition.nearZ;
                }
            }
        }

        // Scan the list of orbit paths and find the closest one. We'll need
        // adjust the nearest interval to accommodate it.
        float zNearest = prevNear;
        for (i = 0; i < (int)orbitPathList.size(); i++) {
            const OrbitPathListEntry& o = orbitPathList[i];
            float minNearDistance = min(-MinNearPlaneDistance, o.centerZ + o.radius);
            if (minNearDistance > zNearest)
                zNearest = minNearDistance;
        }

        // Adjust the nearest interval to include the closest marker (if it's
        // closer to the observer than anything else
        if (!depthSortedAnnotations.empty()) {
            // Factor of 0.999 makes sure ensures that the near plane does not fall
            // exactly at the marker's z coordinate (in which case the marker
            // would be susceptible to getting clipped.)
            if (-depthSortedAnnotations[0].position.z() > zNearest)
                zNearest = -depthSortedAnnotations[0].position.z() * 0.999f;
        }

#if DEBUG_COALESCE
        clog << "nEntries: " << nEntries << ",   zNearest: " << zNearest << ",   prevNear: " << prevNear << "\n";
#endif

        // If the nearest distance wasn't set, nothing should appear
        // in the frontmost depth buffer interval (so we can set the near plane
        // of the front interval to whatever we want as long as it's less than
        // the far plane distance.
        if (zNearest == prevNear)
            zNearest = 0.0f;

        // Add one last interval for the span from 0 to the front of the
        // nearest object
        {
            // TODO: closest object may not be at entry 0, since objects are
            // sorted by far distance.
            float closest = zNearest;
            if (nEntries > 0) {
                closest = max(closest, renderList[0].nearZ);

                // Setting a the near plane distance to zero results in unreliable rendering, even
                // if we don't care about the depth buffer. Compromise and set the near plane
                // distance to a small fraction of distance to the nearest object.
                if (closest == 0.0f) {
                    closest = renderList[0].nearZ * 0.01f;
                }
            }

            DepthBufferPartition partition;
            partition.index = nIntervals;
            partition.nearZ = closest;
            partition.farZ = prevNear;
            depthPartitions.push_back(partition);

            nIntervals++;
        }

        // If orbits are enabled, adjust the farthest partition so that it
        // can contain the orbit.
        if (!orbitPathList.empty()) {
            depthPartitions[0].farZ = min(depthPartitions[0].farZ, orbitPathList[orbitPathList.size() - 1].centerZ -
                                                                       orbitPathList[orbitPathList.size() - 1].radius);
        }

        // We want to avoid overpartitioning the depth buffer. In this stage, we coalesce
        // partitions that have small spans in the depth buffer.
        // TODO: Implement this step!

        vector<Annotation>::iterator annotation = depthSortedAnnotations.begin();

        // Render everything that wasn't culled.
        float intervalSize = 1.0f / (float)max(1, nIntervals);
        i = nEntries - 1;
        for (int interval = 0; interval < nIntervals; interval++) {
            currentIntervalIndex = interval;
            beginObjectAnnotations();

            float nearPlaneDistance = -depthPartitions[interval].nearZ;
            float farPlaneDistance = -depthPartitions[interval].farZ;

            // Set the depth range for this interval--each interval is allocated an
            // equal section of the depth buffer.
            glDepthRange(1.0f - (float)(interval + 1) * intervalSize, 1.0f - (float)interval * intervalSize);

            // Set up a perspective projection using the current interval's near and
            // far clip planes.
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            gluPerspective(fov, (float)windowWidth / (float)windowHeight, nearPlaneDistance, farPlaneDistance);
            glMatrixMode(GL_MODELVIEW);

            Frustum intervalFrustum(degToRad(fov), (float)windowWidth / (float)windowHeight, -depthPartitions[interval].nearZ,
                                    -depthPartitions[interval].farZ);

#if DEBUG_COALESCE
            clog << "interval: " << interval << ", near: " << -depthPartitions[interval].nearZ
                 << ", far: " << -depthPartitions[interval].farZ << "\n";
#endif
            int firstInInterval = i;

            // Render just the opaque objects in the first pass
            while (i >= 0 && renderList[i].farZ < depthPartitions[interval].nearZ) {
                // This interval should completely contain the item
                // Unless it's just a point?
                //assert(renderList[i].nearZ <= depthPartitions[interval].near);

#if DEBUG_COALESCE
                switch (renderList[i].renderableType) {
                    case RenderListEntry::RenderableBody:
                        if (renderList[i].discSizeInPixels > 1) {
                            clog << renderList[i].body->getName() << "\n";
                        } else {
                            clog << "point: " << renderList[i].body->getName() << "\n";
                        }
                        break;

                    case RenderListEntry::RenderableStar:
                        if (renderList[i].discSizeInPixels > 1) {
                            clog << "Star\n";
                        } else {
                            clog << "point: "
                                 << "Star"
                                 << "\n";
                        }
                        break;

                    default:
                        break;
                }
#endif
                // Treat objects that are smaller than one pixel as transparent and render
                // them in the second pass.
                if (renderList[i].isOpaque && renderList[i].discSizeInPixels > 1.0f)
                    renderItem(renderList[i], observer, m_cameraOrientation, nearPlaneDistance, farPlaneDistance);

                i--;
            }

            // Render orbit paths
            if (!orbitPathList.empty()) {
                glDisable(GL_LIGHTING);
                glDisable(GL_TEXTURE_2D);
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
#ifdef USE_HDR
                glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
#else
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif
                if ((renderFlags & ShowSmoothLines) != 0) {
                    enableSmoothLines();
                }

                // Scan through the list of orbits and render any that overlap this interval
                for (vector<OrbitPathListEntry>::const_iterator orbitIter = orbitPathList.begin();
                     orbitIter != orbitPathList.end(); orbitIter++) {
                    // Test for overlap
                    float nearZ = -orbitIter->centerZ - orbitIter->radius;
                    float farZ = -orbitIter->centerZ + orbitIter->radius;

                    // Don't render orbits when they're completely outside this
                    // depth interval.
                    if (nearZ < farPlaneDistance && farZ > nearPlaneDistance) {
#ifdef DEBUG_COALESCE
                        switch (interval % 6) {
                            case 0:
                                glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
                                break;
                            case 1:
                                glColor4f(1.0f, 1.0f, 0.0f, 1.0f);
                                break;
                            case 2:
                                glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
                                break;
                            case 3:
                                glColor4f(0.0f, 1.0f, 1.0f, 1.0f);
                                break;
                            case 4:
                                glColor4f(0.0f, 0.0f, 1.0f, 1.0f);
                                break;
                            case 5:
                                glColor4f(1.0f, 0.0f, 1.0f, 1.0f);
                                break;
                            default:
                                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                                break;
                        }
#endif
                        orbitsRendered++;
                        renderOrbit(*orbitIter, now, m_cameraOrientation.cast<double>(), intervalFrustum, nearPlaneDistance,
                                    farPlaneDistance);

#if DEBUG_COALESCE
                        if (highlightObject.body() == orbitIter->body) {
                            clog << "orbit, radius=" << orbitIter->radius << "\n";
                        }
#endif
                    } else
                        orbitsSkipped++;
                }

                if ((renderFlags & ShowSmoothLines) != 0)
                    disableSmoothLines();
                glDepthMask(GL_FALSE);
            }

            // Render transparent objects in the second pass
            i = firstInInterval;
            while (i >= 0 && renderList[i].farZ < depthPartitions[interval].nearZ) {
                if (!renderList[i].isOpaque || renderList[i].discSizeInPixels <= 1.0f)
                    renderItem(renderList[i], observer, m_cameraOrientation, nearPlaneDistance, farPlaneDistance);

                i--;
            }

            // Render annotations in this interval
            if ((renderFlags & ShowSmoothLines) != 0)
                enableSmoothLines();
            annotation = renderSortedAnnotations(annotation, -depthPartitions[interval].nearZ, -depthPartitions[interval].farZ,
                                                 FontNormal);
            endObjectAnnotations();
            if ((renderFlags & ShowSmoothLines) != 0)
                disableSmoothLines();
            glDisable(GL_DEPTH_TEST);
        }
#if 0
    // TODO: Debugging output for new orbit code; remove when development is complete
    clog << "orbits: " << orbitsRendered
        << ", skipped: " << orbitsSkipped
        << ", sections culled: " << sectionsCulled
        << ", nIntervals: " << nIntervals << "\n";
#endif
        orbitsRendered = 0;
        orbitsSkipped = 0;
        sectionsCulled = 0;

        // reset the depth range
        glDepthRange(0, 1);
    }

    renderForegroundAnnotations(FontNormal);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(fov, (float)windowWidth / (float)windowHeight, NEAR_DIST, FAR_DIST);
    glMatrixMode(GL_MODELVIEW);

    if (!selectionVisible && (renderFlags & ShowMarkers))
        renderSelectionPointer(observer, now, xfrustum, sel);

    // Pop camera orientation matrix
    glPopMatrix();

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPolygonMode(GL_FRONT, GL_FILL);
    glPolygonMode(GL_BACK, GL_FILL);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);

#if 0
int errCode = glGetError();
if (errCode != GL_NO_ERROR) {
    cout << "glError: " << (char*)gluErrorString(errCode) << '\n';
}
#endif

#ifdef VIDEO_SYNC
    if (videoSync && glXWaitVideoSyncSGI != NULL) {
        unsigned int count;
        glXGetVideoSyncSGI(&count);
        glXWaitVideoSyncSGI(2, (count + 1) & 1, &count);
    }
#endif
}
#endif
