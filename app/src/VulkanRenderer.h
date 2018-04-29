#pragma once
#include <QtCore/QObject>
#include <celengine/render.h>
#include <vks/context.hpp>
#include <vks/swapchain.hpp>

class QWindow;
class QTimer;

class VulkanRenderer : public QObject, public Renderer {
    using Parent = Renderer;

public:
    VulkanRenderer(QWindow* window) : _window(window) {}
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
    vk::CommandBuffer getFrameCommandBuffer();
    void waitIdle();

private:
    void renderSkyGrids(const Observer&);
    void renderDeepSkyObjects(const Universe& universe, const Observer& observer, const float faintestMagNight);
    void renderStars(const StarDatabase& starDB, float faintestMagNight, const Observer& observer);

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

    struct Camera {
        glm::mat4 projection;
        glm::mat4 view;
    } _camera;

    std::vector<glm::mat4> _transforms;

    struct {
        vks::Buffer transforms;
        vks::Buffer cameraUbo;
    } _buffers;

    struct SkyGrids {
        struct PushContstants {
            glm::mat4 orientation;
            glm::vec4 color{ 1, 1, 1, 1 };
        };
        vk::Pipeline pipeline;
        vks::Buffer vertices;
        vks::Buffer indices;
        vk::PipelineLayout pipelineLayout;
        vk::DescriptorSet descriptorSet;
        vk::DescriptorSetLayout descriptorSetLayout;
    } _skyGrids;
};
