#pragma once
#include <QtCore/QObject>
#include <QtGui/QWindow>

#include <celengine/render.h>
#include <vks/context.hpp>
#include <vks/swapchain.hpp>

class QTimer;

class QResizableWindow : public QWindow {
    Q_OBJECT
protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void resizing();
};

class VulkanRenderer : public QObject, public Renderer {
    Q_OBJECT
    using Parent = Renderer;

public:
    VulkanRenderer(QResizableWindow* window);
    void initialize() override;
    void render(const ObserverPtr&, const UniversePtr&, float faintestVisible, const Selection& sel) override;
    void shutdown() override;

private slots:
    void onWindowResized();
    void onResizeTimer();
    void onWindowClosing();

private:
    void createRenderPass();
    void createFramebuffers();
    void createDescriptorPool();
    void waitIdle();

private:
    void renderSkyGrids(const Observer&);
    void renderDeepSkyObjects(const Universe& universe, const Observer& observer, const float faintestMagNight);
    void renderStars(const Observer& observer, const StarDatabase& starDB, float faintestMagNight);

private:
    QTimer* _resizeTimer{ nullptr };
    bool _resizing{ false };
    bool _ready{ false };
    QWindow* _window{ nullptr };
    vks::Context _context;
    vk::Extent2D _extent;
    const vk::Device& _device{ _context.device };
    const vk::Queue& _queue{ _context.queue };
    vks::Swapchain _swapchain;
    vk::RenderPass _renderPass;
    vk::DescriptorPool _descriptorPool;
    struct {
        vk::Framebuffer framebuffer;
        vk::CommandBuffer commandBuffer;
    } _frame;
    std::vector<vk::Framebuffer> _framebuffers;

    struct {
        vk::Semaphore acquireComplete;
        vk::Semaphore renderComplete;
    } _semaphores;

    struct CameraData {
        struct Matrices {
            glm::mat4 projection;
            glm::mat4 view;
        } matrices;
        vks::Buffer ubo;
        vk::DescriptorSetLayout descriptorSetLayout;
        vk::DescriptorSet descriptorSet;
    } _camera;

    struct SkyGrids {
        struct PushContstants {
            glm::mat4 orientation;
            glm::vec4 color{ 1, 1, 1, 1 };
        } pushConstants;
        vk::Pipeline pipeline;
        vk::PipelineLayout pipelineLayout;
        vks::Buffer vertices;
        vks::Buffer indices;
        uint32_t indexCount;
        void setup(const vks::Context& context, const vk::RenderPass& renderPass, const vk::ArrayProxy<const vk::DescriptorSetLayout>& layouts);
        void render(const vk::CommandBuffer& commandBuffer, const vk::ArrayProxy<const vk::DescriptorSet>& descriptorSets, uint32_t renderFlags);
    } _skyGrids;

    struct Stars {
        vk::Pipeline starPipeline;
        vk::PipelineLayout pipelineLayout;
        vk::Pipeline glarePipeline;
        vks::Buffer glareVertices;
        uint32_t glareVertexCount{ 0 };
        vks::Buffer starVertices;
        uint32_t starVertexCount{ 0 };

        void setup(const vks::Context& context, const vk::RenderPass& renderPass, const vk::ArrayProxy<const vk::DescriptorSetLayout>& layouts);
        void update(const StarDatabase& starDB);
        void render(const vk::CommandBuffer& commandBuffer, const vk::ArrayProxy<const vk::DescriptorSet>& descriptorSets);
    } _stars;
};
