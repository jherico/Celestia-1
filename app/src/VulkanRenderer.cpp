#include "VulkanRenderer.h"

#include <mutex>

#include <glm/gtc/matrix_transform.hpp>  // glm::translate, glm::rotate, glm::scale, glm::perspective
#include <gli/texture.hpp>
#include <gli/texture2d.hpp>

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

float fov = TAUf / 6.0f;
float aspectRatio = 1.0f;

glm::vec3 toGlm(const Eigen::Vector3f& v) {
    return glm::vec3(v.x(), v.y(), v.z());
}

template <typename PREC>
glm::tquat<PREC, glm::highp> toGlm(const Eigen::Quaternion<PREC>& q) {
    return glm::tquat<PREC, glm::highp>(q.w(), q.x(), q.y(), q.z());
}

template <typename PREC>
glm::mat4 toMat4(const Eigen::Quaternion<PREC>& q) {
    return glm::mat4_cast(glm::quat(toGlm<float>(q.cast<float>())));
}

glm::vec4 toGlm(const Color& color) {
    return glm::vec4(color.red(), color.green(), color.blue(), color.alpha());
}

static Vector3d toStandardCoords(const Vector3d& v) {
    return Vector3d(v.x(), -v.z(), v.y());
}

StarOctree::Frustum computeFrustum(const Eigen::Vector3f& position, const Eigen::Quaternionf& orientation, float fovY, float aspectRatio) {
    // Compute the bounding planes of an infinite view frustum
    StarOctree::Frustum frustumPlanes;
    Vector3f planeNormals[5];
    Eigen::Matrix3f rot = orientation.toRotationMatrix();
    float h = (float)tan(fovY / 2);
    float w = h * aspectRatio;
    planeNormals[0] = Vector3f(0.0f, 1.0f, -h);
    planeNormals[1] = Vector3f(0.0f, -1.0f, -h);
    planeNormals[2] = Vector3f(1.0f, 0.0f, -w);
    planeNormals[3] = Vector3f(-1.0f, 0.0f, -w);
    planeNormals[4] = Vector3f(0.0f, 0.0f, -1.0f);
    for (int i = 0; i < 5; i++) {
        planeNormals[i] = rot.transpose() * planeNormals[i].normalized();
        frustumPlanes[i] = Hyperplane<float, 3>(planeNormals[i], position);
    }
    return frustumPlanes;
}

VulkanRenderer::VulkanRenderer(QWindow* window)
    : _window(window) {
}

void VulkanRenderer::onWindowResized() {
    if (!_resizing) {
        _resizing = true;
    }
    _resizeTimer->start();
}

void VulkanRenderer::onResizeTimer() {
    waitIdle();
    _resizing = false;
    for (const auto& framebuffer : _framebuffers) {
        _device.destroy(framebuffer);
    }
    _framebuffers.clear();

    auto windowSize = _window->geometry().size();
    _extent = vk::Extent2D{ (uint32_t)windowSize.width(), (uint32_t)windowSize.height() };
    aspectRatio = (float)_extent.width / (float)_extent.height;
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
    _resizeTimer->setInterval(100);
    _resizeTimer->setSingleShot(true);

    setStarColorTable(GetStarColorTable(ColorTable_Enhanced));
    QObject::connect(_resizeTimer, &QTimer::timeout, this, &VulkanRenderer::onResizeTimer);

    {
        auto windowSize = _window->geometry().size();
        _extent = vk::Extent2D{ (uint32_t)windowSize.width(), (uint32_t)windowSize.height() };
        aspectRatio = (float)_extent.width / (float)_extent.height;
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
        _camera.ubo = _context.createUniformBuffer(_camera.cameras);
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
    _skyGrids.setup(*this);
    _stars.setup(*this);
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

template <class OBJ, class PREC>
class ObjectRenderer : public OctreeProcessor<OBJ, PREC> {
public:
    ObjectRenderer(const Observer& observer, const PREC distanceLimit)
        : observer(observer)
        , distanceLimit((float)distanceLimit) {}

public:
    const Observer& observer;

    Eigen::Vector3f viewNormal;

    float size{ 0 };
    float pixelSize{ 0 };
    float faintestMag{ 0 };
    float faintestMagNight{ 0 };
    float saturationMag{ 0 };
#ifdef USE_HDR
    float exposure{ 0 };
#endif
    float brightnessScale{ 0 };
    float brightnessBias{ 0 };
    float distanceLimit{ 0 };

    // These are not fully used by this template's descendants
    // but we place them here just in case a more sophisticated
    // rendering scheme is implemented:
    int nRendered{ 0 };
    int nClose{ 0 };
    int nBright{ 0 };
    int nProcessed{ 0 };
    int nLabelled{ 0 };

    int renderFlags{ 0 };
    int labelMode{ 0 };
};

static const float STAR_DISTANCE_LIMIT = 1.0e6f;

// Convert a position in the universal coordinate system to astrocentric
// coordinates, taking into account possible orbital motion of the star.
static Vector3d astrocentricPosition(const UniversalCoord& pos, const Star& star, double t) {
    return pos.offsetFromKm(star.getPosition(t));
}

static const float RenderDistance = 50.0f;
// Maximum size of a solar system in light years. Features beyond this distance
// will not necessarily be rendered correctly. This limit is used for
// visibility culling of solar systems.
static const float MaxSolarSystemSize = 1.0f;
static const float MaxScaledDiscStarSize = 8.0f;
static const float GlareOpacity = 0.65f;
static const float BaseStarDiscSize = 5.0f;

//
// Star rendering
//

class PointStarRenderer : public ObjectRenderer<Star, float> {
public:
    PointStarRenderer(const Observer& observer, const StarDatabase& starDB)
        : ObjectRenderer<Star, float>(observer, STAR_DISTANCE_LIMIT)
        , starDB(starDB) {}

    void process(const StarPtr& star, float distance, float appMag) override;

public:
    Vector3d obsPos;
    //std::vector<RenderListEntry> renderList;
    const StarDatabase& starDB;
    bool useScaledDiscs{ true };
    float maxDiscSize{ 1 };
    std::vector<size_t> indices;

    struct StarVertex {
        glm::vec4 positionAndSize;
        glm::vec4 color;
    };

    using StarVertices = std::vector<StarVertex>;

    std::vector<StarVertex> glareVertexData;
    std::vector<StarVertex> starVertexData;

    const ColorTemperatureTable* colorTemp{ nullptr };

protected:
    static void addStarVertex(StarVertices& outVertices, const Vector3f& relativePosition, const Color& color, float size) {
        outVertices.push_back(StarVertex{
            glm::vec4(relativePosition.x(), relativePosition.y(), relativePosition.z(), size),
            glm::vec4(color.red(), color.green(), color.blue(), color.alpha()),
        });
    }

    void addStarVertex(const Vector3f& relativePosition, const Color& color, float size) { addStarVertex(starVertexData, relativePosition, color, size); }
    void addGlareVertex(const Vector3f& relativePosition, const Color& color, float size) { addStarVertex(glareVertexData, relativePosition, color, size); }
};

void PointStarRenderer::process(const StarPtr& starPtr, float distance, float appMag) {
    nProcessed++;
    const auto& star = *starPtr;
    auto starPos = star.getPosition();

    // Calculate the difference at double precision *before* converting to float.
    // This is very important for stars that are far from the origin.
    Vector3f relPos = (starPos.cast<double>() - obsPos).cast<float>();
    float orbitalRadius = star.getOrbitalRadius();
    bool hasOrbit = orbitalRadius > 0.0f;

    if (distance > distanceLimit)
        return;

    // A very rough check to see if the star may be visible: is the star in
    // front of the viewer? If the star might be close (relPos.x^2 < 0.1) or
    // is moving in an orbit, we'll always regard it as potentially visible.
    // TODO: consider normalizing relPos and comparing relPos*viewNormal against
    // cosFOV--this will cull many more stars than relPos*viewNormal, at the
    // cost of a normalize per star.
    if (relPos.dot(viewNormal) > 0.0f || relPos.x() * relPos.x() < 0.1f || hasOrbit) {
        Color starColor = colorTemp->lookupColor(star.getTemperature());
        float discSizeInPixels = 0.0f;
        float orbitSizeInPixels = 0.0f;

        if (hasOrbit)
            orbitSizeInPixels = orbitalRadius / (distance * pixelSize);

        // Special handling for stars less than one light year away . . .
        // We can't just go ahead and render a nearby star in the usual way
        // for two reasons:
        //   * It may be clipped by the near plane
        //   * It may be large enough that we should render it as a mesh
        //     instead of a particle
        // It's possible that the second condition might apply for stars
        // further than one light year away if the star is huge, the fov is
        // very small and the resolution is high.  We'll ignore this for now
        // and use the most inexpensive test possible . . .
        if (distance < 1.0f || orbitSizeInPixels > 1.0f) {
            // Compute the position of the observer relative to the star.
            // This is a much more accurate (and expensive) distance
            // calculation than the previous one which used the observer's
            // position rounded off to floats.
            Vector3d hPos = astrocentricPosition(observer.getPosition(), star, observer.getTime());
            relPos = hPos.cast<float>() * -astro::kilometersToLightYears(1.0f), distance = relPos.norm();

            // Recompute apparent magnitude using new distance computation
            appMag = astro::absToAppMag(star.getAbsoluteMagnitude(), distance);

            float f = RenderDistance / distance;
            starPos = obsPos.cast<float>() + relPos * f;

            float radius = star.getRadius();
            discSizeInPixels = radius / astro::lightYearsToKilometers(distance) / pixelSize;
            ++nClose;
        }

        // Stars closer than the maximum solar system size are actually
        // added to the render list and depth sorted, since they may occlude
        // planets.
        if (distance > MaxSolarSystemSize) {
            float satPoint = faintestMag - (1.0f - brightnessBias) / brightnessScale;  // TODO: precompute this value
            float alpha = (faintestMag - appMag) * brightnessScale + brightnessBias;
            if (useScaledDiscs) {
                float discSize = size;
                if (alpha < 0.0f) {
                    alpha = 0.0f;
                } else if (alpha > 1.0f) {
                    float discScale = std::min(MaxScaledDiscStarSize, (float)pow(2.0f, 0.3f * (satPoint - appMag)));
                    discSize *= discScale;

                    float glareAlpha = std::min(0.5f, discScale / 4.0f);
                    addGlareVertex(relPos, Color(starColor, glareAlpha), discSize * 3.0f);
                    alpha = 1.0f;
                }
                addStarVertex(relPos, Color(starColor, alpha), discSize);
            } else {
                if (alpha < 0.0f) {
                    alpha = 0.0f;
                } else if (alpha > 1.0f) {
                    float discScale = std::min(100.0f, satPoint - appMag + 2.0f);
                    float glareAlpha = std::min(GlareOpacity, (discScale - 2.0f) / 4.0f);
                    addGlareVertex(relPos, Color(starColor, glareAlpha), 2.0f * discScale * size);
                }
                addStarVertex(relPos, Color(starColor, alpha), size);
            }
            ++nRendered;
        } else {
#if 0
            Matrix3f viewMat = observer.getOrientationf().toRotationMatrix();
            Vector3f viewMatZ = viewMat.row(2);

            RenderListEntry rle;
            rle.renderableType = RenderListEntry::RenderableStar;
            rle.star = starPtr;

            // Objects in the render list are always rendered relative to
            // a viewer at the origin--this is different than for distant
            // stars.
            float scale = astro::lightYearsToKilometers(1.0f);
            rle.position = relPos * scale;
            rle.centerZ = rle.position.dot(viewMatZ);
            rle.distance = rle.position.norm();
            rle.radius = star.getRadius();
            rle.discSizeInPixels = discSizeInPixels;
            rle.appMag = appMag;
            rle.isOpaque = true;
            renderList.push_back(rle);
#endif
        }
    }
}

#if 0
static void BuildGaussianDiscMipLevel(gli::texture2d& texture, uint32_t size, uint32_t mipLevel, float fwhm, float power) {
    float sigma = fwhm / 2.3548f;
    float isig2 = 1.0f / (2.0f * sigma * sigma);
    float s = 1.0f / (sigma * (float)sqrt(2.0 * PI));

    for (unsigned int i = 0; i < size; i++) {
        float y = (float)i - (float)size / 2.0f + 0.5f;
        for (unsigned int j = 0; j < size; j++) {
            float x = (float)j - (float)size / 2.0f + 0.5f;
            float r2 = x * x + y * y;
            float f = s * (float)exp(-r2 * isig2) * power;
            texture.store({ j, i }, mipLevel, (unsigned char)(255.99f * std::min(f, 1.0f)));
        }
    }
}

static void BuildGlareMipLevel(gli::texture2d& texture, uint32_t size, uint16_t mipLevel, float scale, float base) {
    for (uint32_t i = 0; i < size; i++) {
        float y = (float)i - (float)size / 2.0f + 0.5f;
        for (unsigned int j = 0; j < size; j++) {
            float x = (float)j - (float)size / 2.0f + 0.5f;
            float r = (float)sqrt(x * x + y * y);
            float f = (float)pow(base, r * scale);
            texture.store({ j, i }, mipLevel, (unsigned char)(255.99f * std::min(f, 1.0f)));
        }
    }
}

static std::shared_ptr<gli::texture2d> BuildGaussianDiscTexture(unsigned int log2size) {
    unsigned int size = 1 << log2size;
    auto result = std::make_shared<gli::texture2d>(gli::format::FORMAT_L8_UNORM_PACK8, gli::texture2d::extent_type{ size, size }, log2size + 1);
    for (unsigned int mipLevel = 0; mipLevel <= log2size; mipLevel++) {
        float fwhm = (float)pow(2.0f, (float)(log2size - mipLevel)) * 0.3f;
        uint32_t mipSize = size >> mipLevel;
        BuildGaussianDiscMipLevel(*result, mipSize, mipLevel, fwhm, (float)pow(2.0f, (float)(log2size - mipLevel)));
    }
    return result;
}

static std::shared_ptr<gli::texture2d> BuildGaussianGlareTexture(unsigned int log2size) {
    unsigned int size = 1 << log2size;
    auto result = std::make_shared<gli::texture2d>(gli::format::FORMAT_L8_UNORM_PACK8, gli::texture2d::extent_type{ size, size }, log2size + 1);
    for (unsigned int mipLevel = 0; mipLevel <= log2size; mipLevel++) {
        uint32_t mipSize = size >> mipLevel;
        BuildGlareMipLevel(*result, mipSize, mipLevel, 25.0f / (float)pow(2.0f, (float)(log2size - mipLevel)), 0.66f);
    }
    return result;
}
#endif

void VulkanRenderer::Stars::setup(VulkanRenderer& renderer) {
    // Pipeline layout
    const auto& context = renderer._context;

    gaussianDiscTex.loadFromFile(context, getAssetPath() + "textures/ktx/gaussianDisc.ktx", vk::Format::eR8Unorm);
    gaussianGlareTex.loadFromFile(context, getAssetPath() + "textures/ktx/gaussianGlare.ktx", vk::Format::eR8Unorm);

    {
        std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings{
            vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
        };
        descriptorSetLayout = context.device.createDescriptorSetLayout({ {}, (uint32_t)setLayoutBindings.size(), setLayoutBindings.data() });

        // Example uses one ubo and one image sampler
        std::vector<vk::DescriptorPoolSize> poolSizes = {
            vk::DescriptorPoolSize{ vk::DescriptorType::eCombinedImageSampler, 1 },
        };
        descriptorPool = context.device.createDescriptorPool({ {}, 2, (uint32_t)poolSizes.size(), poolSizes.data() });
        starDescriptorSet = context.device.allocateDescriptorSets({ descriptorPool, 1, &descriptorSetLayout })[0];
        glareDescriptorSet = context.device.allocateDescriptorSets({ descriptorPool, 1, &descriptorSetLayout })[0];
        // vk::Image descriptor for the color map texture
        vk::DescriptorImageInfo glareTexDescriptor{ gaussianGlareTex.sampler, gaussianGlareTex.view, vk::ImageLayout::eGeneral };
        vk::DescriptorImageInfo discTexDescriptor{ gaussianDiscTex.sampler, gaussianDiscTex.view, vk::ImageLayout::eGeneral };
        context.device.updateDescriptorSets(
            {
                { starDescriptorSet, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &discTexDescriptor },
                { glareDescriptorSet, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &glareTexDescriptor },
            },
            nullptr);
    }

    {
        vk::ArrayProxy<const vk::DescriptorSetLayout> layouts{ renderer._camera.descriptorSetLayout, descriptorSetLayout };
        pipelineLayout = context.device.createPipelineLayout(vk::PipelineLayoutCreateInfo{ {}, layouts.size(), layouts.data(), 0, nullptr });
        vks::pipelines::GraphicsPipelineBuilder builder{ context.device, context.pipelineCache };
        builder.layout = pipelineLayout;
        builder.renderPass = renderer._renderPass;
        builder.inputAssemblyState.topology = vk::PrimitiveTopology::ePointList;
        builder.vertexInputState.bindingDescriptions = { { 0, sizeof(PointStarRenderer::StarVertex), vk::VertexInputRate::eVertex } };
        builder.vertexInputState.attributeDescriptions = {
            { 0, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(PointStarRenderer::StarVertex, positionAndSize) },
            { 1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(PointStarRenderer::StarVertex, color) },
        };
        builder.dynamicState.dynamicStateEnables = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
        };

        auto& blendAttachmentState = builder.colorBlendState.blendAttachmentStates[0];
        // Additive blending
        blendAttachmentState.blendEnable = VK_TRUE;
        blendAttachmentState.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        blendAttachmentState.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        blendAttachmentState.colorBlendOp = vk::BlendOp::eAdd;
        blendAttachmentState.srcAlphaBlendFactor = vk::BlendFactor::eSrcAlpha;
        blendAttachmentState.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        blendAttachmentState.alphaBlendOp = vk::BlendOp::eAdd;
        builder.loadShader(getAssetPath() + "shaders/stars.vert.spv", vk::ShaderStageFlagBits::eVertex);
        builder.loadShader(getAssetPath() + "shaders/stars.frag.spv", vk::ShaderStageFlagBits::eFragment);
        starPipeline = builder.create();
    }
}

void VulkanRenderer::Stars::render(const vk::CommandBuffer& commandBuffer, const vk::ArrayProxy<const vk::DescriptorSet>& descriptorSets) {
    if (starVertexCount != 0 || glareVertexCount != 0) {
        vks::debug::marker::beginRegion(commandBuffer, "stars");
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, starPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSets, nullptr);
        if (starVertexCount != 0) {
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, starDescriptorSet, nullptr);
            commandBuffer.bindVertexBuffers(0, starVertices.buffer, { 0 });
            commandBuffer.draw(starVertexCount, 1, 0, 0);
        }

        if (glareVertexCount != 0) {
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, glareDescriptorSet, nullptr);
            commandBuffer.bindVertexBuffers(0, glareVertices.buffer, { 0 });
            commandBuffer.draw(glareVertexCount, 1, 0, 0);
        }
        vks::debug::marker::endRegion(commandBuffer);
    }
}

void VulkanRenderer::renderStars(const Observer& observer, const StarDatabase& starDB, float faintestMagNight) {
    //_context.deviceFeatures.largePoints
    Vector3d obsPos = observer.getPosition().toLy();

    PointStarRenderer starRenderer(observer, starDB);
    starRenderer.obsPos = obsPos;
    starRenderer.viewNormal = observer.getOrientationf().conjugate() * -Vector3f::UnitZ();

    starRenderer.pixelSize = pixelSize;
    starRenderer.brightnessScale = brightnessScale * corrFac;
    starRenderer.brightnessBias = brightnessBias;
    starRenderer.faintestMag = faintestMag;
    starRenderer.faintestMagNight = faintestMagNight;
    starRenderer.saturationMag = saturationMag;
    starRenderer.distanceLimit = distanceLimit;
    starRenderer.labelMode = labelMode;
    starRenderer.colorTemp = getStarColorTable();

    // = 1.0 at startup
    //starRenderer.labelThresholdMag = 1.2f * std::max(1.0f, (faintestMag - 4.0f) * (1.0f - 0.5f * (float)log10(effDistanceToScreen)));

    starRenderer.size = BaseStarDiscSize;
    //if (starStyle == ScaledDiscStars) {
    //    starRenderer.useScaledDiscs = true;
    //    starRenderer.brightnessScale *= 2.0f;
    //    starRenderer.maxDiscSize = starRenderer.size * MaxScaledDiscStarSize;
    //} else if (starStyle == FuzzyPointStars) {
    //    starRenderer.brightnessScale *= 1.0f;
    //}
    //starRenderer.colorTemp = colorTemp;
    auto frustum = computeFrustum(obsPos.cast<float>(), observer.getOrientationf(), fov, aspectRatio);
    starDB.findVisibleStars(starRenderer, obsPos.cast<float>(), observer.getOrientationf(), frustum, faintestMagNight);

    auto updateStarVertices = [&](const PointStarRenderer::StarVertices& data, uint32_t& vertexCount, vks::Buffer& vertexBuffer) {
        uint32_t newSize = (uint32_t)data.size();
        if (newSize > vertexCount) {
            if (vertexBuffer) {
                vertexBuffer.unmap();
                _context.trash(vertexBuffer);
            }
            vertexBuffer = _context.createBuffer(vk::BufferUsageFlagBits::eVertexBuffer,
                                                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                                 sizeof(PointStarRenderer::StarVertex) * newSize);
            vertexBuffer.map();
        }
        vertexCount = newSize;
        vertexBuffer.copy(data);
    };

    updateStarVertices(starRenderer.starVertexData, _stars.starVertexCount, _stars.starVertices);
    updateStarVertices(starRenderer.glareVertexData, _stars.glareVertexCount, _stars.glareVertices);
    _stars.render(_frame.commandBuffer, _camera.descriptorSet);
}

//
// Sky grids rendering
//
void VulkanRenderer::SkyGrids::setup(VulkanRenderer& renderer) {
    const auto& context = renderer._context;
    {
        std::vector<glm::vec3> vertices;
        std::vector<uint32_t> indices;
        static const uint32_t MERIDIANS = 36;
        static const uint32_t PARALLELS = 15;
        static const float MAX_ELEVATION = (QUARTER_TAUf * 0.9f);
        static const float MIN_ELEVATION = -MAX_ELEVATION;
        static const float ELEVATION_RANGE = MAX_ELEVATION - MIN_ELEVATION;

        // Meridians
        {
            static const uint32_t MERIDIAN_SEGMENTS = 64;
            static const float AZIMUTH_INTERVAL = TAUf / MERIDIANS;
            static const float ELEVATION_INTERVAL = ELEVATION_RANGE / MERIDIAN_SEGMENTS;

            // For each meridian
            vertices.reserve(vertices.size() + MERIDIANS * (MERIDIAN_SEGMENTS + 1));
            indices.reserve(indices.size() + MERIDIANS * (MERIDIAN_SEGMENTS + 1));
            for (uint32_t i = 0; i < MERIDIANS; ++i) {
                float azimuth = (AZIMUTH_INTERVAL * i) - HALF_TAUf;
                float sinAz = sin(azimuth);
                float cosAz = cos(azimuth);
                // for each
                for (uint32_t j = 0; j <= MERIDIAN_SEGMENTS; ++j) {
                    float elevation = (j * ELEVATION_INTERVAL) + MIN_ELEVATION;
                    elevation = QUARTER_TAUf - elevation;
                    float sinEl = sin(elevation);
                    float cosEl = cos(elevation);
                    glm::vec3 vertex{ sinEl * cosAz, sinEl * sinAz, cosEl };
                    vertex *= 0.5f;
                    indices.push_back((uint32_t)vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(UINT32_MAX);
            }
        }

        // Parallels
        {
            static const uint32_t PARALLEL_SEGMENTS = 64;
            static const float AZIMUTH_INTERVAL = TAUf / PARALLEL_SEGMENTS;
            static const float ELEVATION_INTERVAL = ELEVATION_RANGE / PARALLELS;
            for (uint32_t i = 0; i <= PARALLELS; ++i) {
                float elevation = (i * ELEVATION_INTERVAL) + MIN_ELEVATION;
                elevation = QUARTER_TAUf - elevation;
                float sinEl = sin(elevation);
                float cosEl = cos(elevation);
                // for each
                for (uint32_t j = 0; j <= PARALLEL_SEGMENTS; ++j) {
                    float azimuth = (AZIMUTH_INTERVAL * j) - HALF_TAUf;
                    float sinAz = sin(azimuth);
                    float cosAz = cos(azimuth);
                    glm::vec3 vertex{ sinEl * cosAz, sinEl * sinAz, cosEl };
                    vertex *= 0.5f;
                    indices.push_back((uint32_t)vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(UINT32_MAX);
            }
        }

        this->vertices = context.stageToDeviceBuffer<glm::vec3>(vk::BufferUsageFlagBits::eVertexBuffer, vertices);
        this->indices = context.stageToDeviceBuffer<uint32_t>(vk::BufferUsageFlagBits::eIndexBuffer, indices);
        indexCount = (uint32_t)indices.size();
    }

    // Pipeline layout
    {
        auto& layout = renderer._camera.descriptorSetLayout;
        vk::PushConstantRange pushConstantRange{ vk::ShaderStageFlagBits::eVertex, 0, sizeof(SkyGrids::PushContstants) };
        pipelineLayout = context.device.createPipelineLayout(vk::PipelineLayoutCreateInfo{ {}, 1, &layout, 1, &pushConstantRange });
    }

    {
        vks::pipelines::GraphicsPipelineBuilder builder{ context.device, context.pipelineCache };
        builder.layout = pipelineLayout;
        builder.renderPass = renderer._renderPass;
        builder.inputAssemblyState.primitiveRestartEnable = VK_TRUE;
        builder.inputAssemblyState.topology = vk::PrimitiveTopology::eLineStrip;
        builder.vertexInputState.bindingDescriptions = { { 0, sizeof(glm::vec3), vk::VertexInputRate::eVertex } };
        builder.vertexInputState.attributeDescriptions = { { 0, 0, vk::Format::eR32G32B32Sfloat } };
        builder.dynamicState.dynamicStateEnables = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
            vk::DynamicState::eLineWidth,
        };
        auto& blendAttachmentState = builder.colorBlendState.blendAttachmentStates[0];
        // Additive blending
        blendAttachmentState.blendEnable = VK_TRUE;
        blendAttachmentState.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        blendAttachmentState.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        blendAttachmentState.colorBlendOp = vk::BlendOp::eAdd;
        blendAttachmentState.srcAlphaBlendFactor = vk::BlendFactor::eSrcAlpha;
        blendAttachmentState.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        blendAttachmentState.alphaBlendOp = vk::BlendOp::eAdd;
        builder.loadShader(getAssetPath() + "shaders/skygrid.vert.spv", vk::ShaderStageFlagBits::eVertex);
        builder.loadShader(getAssetPath() + "shaders/skygrid.frag.spv", vk::ShaderStageFlagBits::eFragment);
        pipeline = builder.create();
    }
}

void VulkanRenderer::SkyGrids::render(const vk::CommandBuffer& commandBuffer,
                                      const vk::ArrayProxy<const vk::DescriptorSet>& descriptorSets,
                                      uint32_t renderFlags) {
    vks::debug::marker::beginRegion(commandBuffer, "skyGrids");
    // Submit via push constant (rather than a UBO)
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    if (!descriptorSets.empty()) {
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSets, nullptr);
    }
    commandBuffer.bindVertexBuffers(0, vertices.buffer, { 0 });
    commandBuffer.bindIndexBuffer(indices.buffer, 0, vk::IndexType::eUint32);
    commandBuffer.setLineWidth(1.0f);

    if (renderFlags & ShowCelestialSphere) {
        pushConstants.orientation = toMat4(Quaterniond(AngleAxis<double>(astro::J2000Obliquity, Vector3d::UnitX())));
        pushConstants.color = toGlm(EquatorialGridColor);
        commandBuffer.pushConstants<SkyGrids::PushContstants>(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, pushConstants);
        commandBuffer.drawIndexed(indexCount, 1, 0, 0, 0);
    }

    if (renderFlags & ShowGalacticGrid) {
        pushConstants.orientation = toMat4((astro::eclipticToEquatorial() * astro::equatorialToGalactic()).conjugate());
        pushConstants.color = toGlm(GalacticGridColor);
        commandBuffer.pushConstants<SkyGrids::PushContstants>(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, pushConstants);
        commandBuffer.drawIndexed(indexCount, 1, 0, 0, 0);
    }

    if (renderFlags & ShowEclipticGrid) {
        pushConstants.orientation = glm::mat4();
        pushConstants.color = toGlm(EclipticGridColor);
        commandBuffer.pushConstants<SkyGrids::PushContstants>(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, pushConstants);
        commandBuffer.drawIndexed(indexCount, 1, 0, 0, 0);
    }
    vks::debug::marker::endRegion(commandBuffer);
}

void VulkanRenderer::renderSkyGrids(const Observer& observer) {
    static const auto ALL_GRIDS = ShowCelestialSphere | ShowGalacticGrid | ShowEclipticGrid;

    // DEBUG
    renderFlags |= ShowCelestialSphere;

    if (0 == (renderFlags & ALL_GRIDS)) {
        return;
    }

    _skyGrids.render(_frame.commandBuffer, _camera.descriptorSet, renderFlags);

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

void VulkanRenderer::render(const ObserverPtr& observerPtr, const UniversePtr& universePtr, float faintestVisible, const Selection& sel) {
    if (_resizing || !_ready) {
        return;
    }
    const auto& observer = *observerPtr;
    const auto& universe = *universePtr;
    preRender(observer, universe, faintestVisible, sel);

    _camera.cameras[0].projection = glm::perspective(fov, aspectRatio, 0.1f, 10000.f);
    auto msecs = QDateTime::currentMSecsSinceEpoch() % LOOP_INTERVAL_MS;
    float interval = (float)msecs / (float)LOOP_INTERVAL_MS;
    _camera.cameras[0].view = toMat4(observerPtr->getOrientationf());
    _camera.cameras[0].view = glm::rotate(glm::mat4(), (float)interval * TAUf, glm::vec3{ 1, 0, 0 });
    _camera.ubo.copy(_camera.cameras);

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
        renderStars(observer, *universe.getStarCatalog(), faintestMag);
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
    try {
        _swapchain.queuePresent(_semaphores.renderComplete);
    } catch (const vk::OutOfDateKHRError&) {
        _resizing = true;
        _resizeTimer->start();
    }
    _context.emptyDumpster(fence);
    _context.recycle();
}
