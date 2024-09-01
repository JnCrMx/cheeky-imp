#pragma once

#include "config.hpp"
#include "dispatch.hpp"
#include "rules/rules.hpp"
#include <memory>
#include <spdlog/logger.h>
#include <unordered_map>
#include <unordered_set>
#include <vulkan/vulkan_core.h>
#include <vulkan/utility/vk_dispatch_table.h>
#include <spirv_reflect.h>

#ifdef USE_IMAGE_TOOLS
#include <image.hpp>
#endif

namespace CheekyLayer {

struct instance;

using descriptor_element_info = std::variant<VkDescriptorBufferInfo, VkDescriptorImageInfo>;
struct descriptor_element
{
	rules::VkHandle handle;
	descriptor_element_info info;
};

struct descriptor_binding
{
	CheekyLayer::rules::selector_type type;
	VkDescriptorType exactType;
	std::vector<descriptor_element> arrayElements;
};

struct descriptor_state
{
	std::map<int, descriptor_binding> bindings;
};

struct shader_info
{
	VkShaderStageFlagBits stage;
	VkShaderModule module;
	rules::VkHandle customHandle;
	std::string hash;
	std::string name;
};

struct pipeline_state
{
	std::vector<shader_info> stages;
	std::vector<VkVertexInputBindingDescription> vertexBindingDescriptions;
	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
};

struct buffer_binding
{
	VkBuffer buffer;
	VkDeviceSize offset;
	VkDeviceSize size;
};

struct command_buffer_state
{
	VkDevice device;
	VkPipeline pipeline;
	std::vector<VkDescriptorSet> descriptorSets;
	std::vector<uint32_t> descriptorDynamicOffsets;

	std::vector<VkBuffer> vertexBuffers;
	std::vector<VkDeviceSize> vertexBufferOffsets;

	VkBuffer indexBuffer;
	VkDeviceSize indexBufferOffset;
	VkIndexType indexType;

	std::vector<VkRect2D> scissors;

	VkRenderPass renderpass;
	VkFramebuffer framebuffer;

	bool transformFeedback;
	std::vector<buffer_binding> transformFeedbackBuffers;
};

struct pipeline_layout_info
{
	std::vector<VkDescriptorSetLayout> setLayouts;
	std::vector<VkPushConstantRange> pushConstantRanges;
};

struct framebuffer
{
	std::vector<VkImageView> attachments;
	uint32_t width;
	uint32_t height;

	framebuffer() {}

	framebuffer(const VkFramebufferCreateInfo* i)
	{
		attachments = std::vector<VkImageView>(i->pAttachments, i->pAttachments + i->attachmentCount);
		width = i->width;
		height = i->height;
	}
};

struct device {
    device() = default;
    device(instance* inst, PFN_vkGetDeviceProcAddr gdpa, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, VkDevice *pDevice);

    device& operator=(const device&) = delete; // no copy
    device(const device&) = delete; // no copy

    VkDevice operator*() const { return handle; }

    instance* inst;
    std::shared_ptr<spdlog::logger> logger;

    VkDevice handle;
    PFN_vkGetDeviceProcAddr gdpa;
    VkuDeviceDispatchTable dispatch;

    bool has_debug = false;

    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceMemoryProperties memProperties;
    std::vector<VkQueueFamilyProperties> queueFamilies;

    VkQueue transferQueue = VK_NULL_HANDLE;
    VkCommandPool transferPool;
    VkCommandBuffer transferBuffer;

    struct buffer {
        VkBuffer buffer;
        VkBufferCreateInfo createInfo;
        VkDeviceMemory memory;
        VkDeviceSize memoryOffset;
    };
    struct memory_map_info {
        void* pointer;
        VkDeviceSize offset;
        VkDeviceSize size;
    };
    std::map<VkBuffer, buffer> buffers;
    std::map<VkDeviceMemory, memory_map_info> memoryMappings;

    struct image {
        VkImage image;
        VkImageCreateInfo createInfo;
        VkDeviceMemory memory;
        VkDeviceSize memoryOffset;
        VkImageView view;
#ifdef USE_IMAGE_TOOLS
        std::unique_ptr<image_tools::image> topResolution;
#endif
    };
    std::map<VkImage, image> images;
    std::map<VkImageView, VkImage> imageViewToImage;
    std::map<VkFramebuffer, framebuffer> framebuffers;
    std::map<VkSwapchainKHR, VkSwapchainCreateInfoKHR> swapchains;

    uint64_t currentCustomShaderHandle = 0xABC1230000;
    std::map<rules::VkHandle, rules::VkHandle> customShaderHandles;
    std::map<rules::VkHandle, spv_reflect::ShaderModule> shaderReflections;

    std::map<VkCommandBuffer, command_buffer_state> commandBufferStates;
    std::map<VkPipelineLayout, pipeline_layout_info> pipelineLayouts;
    std::map<VkPipeline, pipeline_state> pipelineStates;

    std::map<VkDescriptorUpdateTemplate, std::vector<VkDescriptorUpdateTemplateEntry>> updateTemplates;
    std::map<VkDescriptorSet, descriptor_state> descriptorStates;

    void execute_rules(rules::selector_type type, rules::VkHandle handle, rules::calling_context& ctx);
    void memory_access(VkDeviceMemory memory, std::function<void(void*, VkDeviceSize)> function, VkDeviceSize offset = 0);
    void memory_access(VkBuffer buffer, std::function<void(void*, VkDeviceSize)> function, VkDeviceSize offset = 0);
    void put_hash(rules::VkHandle handle, std::string hash);
    bool has_override(const std::string& hash);

    void GetDeviceQueue(uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *pQueue);

    // images.cpp
    VkResult CreateImage(const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage*);
    VkResult BindImageMemory(VkImage, VkDeviceMemory, VkDeviceSize);
    VkResult CreateImageView(const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView*);
    void CmdCopyBufferToImage(VkCommandBuffer, VkBuffer, VkImage, VkImageLayout, uint32_t, const VkBufferImageCopy*);

    // buffers.cpp
    VkResult CreateBuffer(const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer*);
    VkResult BindBufferMemory(VkBuffer, VkDeviceMemory, VkDeviceSize);
    VkResult MapMemory(VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void**);
    void UnmapMemory(VkDeviceMemory);
    void CmdCopyBuffer(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t, const VkBufferCopy*);

    // shaders.cpp
    VkResult CreateShaderModule(const VkShaderModuleCreateInfo*, const VkAllocationCallbacks*, VkShaderModule*);

    // descriptors.cpp
    VkResult CreateDescriptorUpdateTemplate(const VkDescriptorUpdateTemplateCreateInfo*, const VkAllocationCallbacks*, VkDescriptorUpdateTemplate*);
    void UpdateDescriptorSetWithTemplate(VkDescriptorSet, VkDescriptorUpdateTemplate, const void*);

    // draw.cpp
    VkResult AllocateCommandBuffers(const VkCommandBufferAllocateInfo*, VkCommandBuffer*);
    void FreeCommandBuffers(VkCommandPool, uint32_t, const VkCommandBuffer*);
    VkResult CreateFramebuffer(const VkFramebufferCreateInfo*, const VkAllocationCallbacks*, VkFramebuffer*);
    VkResult CreatePipelineLayout(const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout*);
    VkResult CreateGraphicsPipelines(VkPipelineCache, uint32_t, const VkGraphicsPipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline*);
    VkResult CreateSwapchainKHR(const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*);
    VkResult GetSwapchainImagesKHR(VkSwapchainKHR, uint32_t*, VkImage*);
    void CmdBindDescriptorSets(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*);
    void CmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
    void CmdBindVertexBuffers(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*);
    void CmdBindVertexBuffers2EXT(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*, const VkDeviceSize*, const VkDeviceSize*);
    void CmdBindIndexBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkIndexType);
    void CmdSetScissor(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D*);
    void CmdBeginRenderPass(VkCommandBuffer, const VkRenderPassBeginInfo*, VkSubpassContents);
    void CmdEndRenderPass(VkCommandBuffer);
    void CmdDraw(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t);
    void CmdDrawIndexed(VkCommandBuffer, uint32_t, uint32_t, uint32_t, int32_t, uint32_t);
    void CmdBeginTransformFeedbackEXT(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*);
    void CmdBindTransformFeedbackBuffersEXT(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*, const VkDeviceSize*);
    void CmdEndTransformFeedbackEXT(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*);
    VkResult EndCommandBuffer(VkCommandBuffer);
    VkResult QueueSubmit(VkQueue, uint32_t, const VkSubmitInfo*, VkFence);
    VkResult QueuePresentKHR(VkQueue, const VkPresentInfoKHR*);
};

struct instance {
    static unsigned int instance_id;

    instance() = default;
    instance(const VkInstanceCreateInfo *pCreateInfo, VkInstance* pInstance);

    instance& operator=(const instance&) = delete; // no copy
    instance(const instance&) = delete; // no copy

    instance& operator=(instance&&);

    VkInstance operator*() const { return handle; }

    unsigned int id;
    VkInstance handle;
    VkuInstanceDispatchTable dispatch;

    CheekyLayer::config config;
    bool enabled = false;
    bool hook_draw_calls = false;

    std::vector<std::unique_ptr<rules::rule>> rules;
    std::map<CheekyLayer::rules::selector_type, bool> has_rules;
    rules::global_context global_context;

    std::unordered_set<std::string> overrideCache;
    std::unordered_set<std::string> dumpCache;

    std::mutex lock;
    std::shared_ptr<spdlog::logger> logger;

    std::unordered_map<VkDevice, device*> devices;

    void execute_rules(rules::selector_type type, rules::VkHandle handle, rules::calling_context& ctx);
    void put_hash(rules::VkHandle handle, std::string hash);

    VkResult CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice);
};

inline std::unordered_map<void*, std::unique_ptr<instance>> instances;
inline std::unordered_map<void*, std::unique_ptr<device>> devices;

inline instance& get_instance(VkInstance instance) {
    return *instances.at(GetKey(instance));
}
inline instance& get_instance(VkPhysicalDevice physicalDevice) {
    return *instances.at(GetKey(physicalDevice));
}
inline device& get_device(VkDevice device) {
    return *devices.at(GetKey(device));
}
inline device& get_device(VkCommandBuffer commandBuffer) {
    return *devices.at(GetKey(commandBuffer));
}
inline device& get_device(VkQueue commandBuffer) {
    return *devices.at(GetKey(commandBuffer));
}

}
