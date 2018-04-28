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
    void createCommandBuffers();
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
    std::vector<vk::Framebuffer> _framebuffers;
    std::vector<vk::CommandBuffer> _commandBuffers;
    struct {
        vk::Semaphore acquireComplete;
        vk::Semaphore renderComplete;
    } _semaphores;
};
