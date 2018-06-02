#pragma once
#include <QtCore/QObject>

#include <celengine/render.h>
#include <vks/context.hpp>
#include <vks/swapchain.hpp>
#include <vks/texture.hpp>

#include <OVR_CAPI_Vk.h>

class QTimer;
class QWindow;

class VulkanRenderer : public QObject, public Renderer {
    Q_OBJECT
    using Parent = Renderer;

public:
    VulkanRenderer(QWindow* window);
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
        struct Camera {
            glm::mat4 projection;
            glm::mat4 view;
        };
        using CameraArray = std::array<Camera, 2>;
        CameraArray cameras;
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
        void setup(VulkanRenderer& renderer);
        void render(const vk::CommandBuffer& commandBuffer, const vk::ArrayProxy<const vk::DescriptorSet>& descriptorSets, uint32_t renderFlags);
    } _skyGrids;

    struct Stars {
        vk::DescriptorPool descriptorPool;
        vk::DescriptorSetLayout descriptorSetLayout;
        vk::PipelineLayout pipelineLayout;
        vk::Pipeline starPipeline;
        vk::DescriptorSet starDescriptorSet;
        vk::Pipeline glarePipeline;
        vk::DescriptorSet glareDescriptorSet;
        vks::Buffer glareVertices;
        uint32_t glareVertexCount{ 0 };
        vks::Buffer starVertices;
        uint32_t starVertexCount{ 0 };
        vks::texture::Texture2D gaussianDiscTex;
        vks::texture::Texture2D gaussianGlareTex;

        void setup(VulkanRenderer& renderer);
        void update(const StarDatabase& starDB);
        void render(const vk::CommandBuffer& commandBuffer, const vk::ArrayProxy<const vk::DescriptorSet>& descriptorSets);
    } _stars;
};
