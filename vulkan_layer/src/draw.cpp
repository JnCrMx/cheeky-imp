#include "dispatch.hpp"
#include "layer.hpp"
#include "descriptors.hpp"
#include "reflection/reflectionparser.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "objects.hpp"

#include <iomanip>
#include <iterator>
#include <experimental/iterator>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

namespace CheekyLayer {

VkResult device::CreateSwapchainKHR(const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain)
{
	VkResult result = dispatch.CreateSwapchainKHR(handle, pCreateInfo, pAllocator, pSwapchain);
	if(result != VK_SUCCESS)
		return result;

	rules::swapchain_info info = {pCreateInfo};
	rules::calling_context ctx{
		.info = info
	};
	execute_rules(rules::selector_type::SwapchainCreate, (VkHandle)(*pSwapchain), ctx);

	return result;
}

VkResult device::AllocateCommandBuffers(const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) {
	VkResult result = dispatch.AllocateCommandBuffers(handle, pAllocateInfo, pCommandBuffers);
	if(result != VK_SUCCESS)
		return result;

	logger->info("AllocateCommandBuffers: {}", pAllocateInfo->commandBufferCount);

	for(int i=0; i<pAllocateInfo->commandBufferCount; i++)
	{
		commandBufferStates[pCommandBuffers[i]] = {handle};

		inst->global_context.on_EndCommandBuffer[pCommandBuffers[i]] = {};
		inst->global_context.on_QueueSubmit[pCommandBuffers[i]] = {};
	}

	return result;
}

void device::FreeCommandBuffers(VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
	logger->info("FreeCommandBuffers: {}", commandBufferCount);

	for(int i=0; i<commandBufferCount; i++)
	{
		commandBufferStates.erase(pCommandBuffers[i]);

		inst->global_context.on_EndCommandBuffer.erase(pCommandBuffers[i]);
		inst->global_context.on_QueueSubmit.erase(pCommandBuffers[i]);
	}

	dispatch.FreeCommandBuffers(handle, commandPool, commandBufferCount, pCommandBuffers);
}

VkResult device::CreateFramebuffer(const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer)
{
	VkResult result = dispatch.CreateFramebuffer(handle, pCreateInfo, pAllocator, pFramebuffer);
	framebuffers[*pFramebuffer] = framebuffer(pCreateInfo);
	return result;
}

VkResult device::CreatePipelineLayout(const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout)
{
	VkResult result = dispatch.CreatePipelineLayout(handle, pCreateInfo, pAllocator, pPipelineLayout);
	if(result != VK_SUCCESS)
		return result;

	pipeline_layout_info& info = pipelineLayouts[*pPipelineLayout];
	info.setLayouts = std::vector<VkDescriptorSetLayout>(pCreateInfo->pSetLayouts, pCreateInfo->pSetLayouts+pCreateInfo->setLayoutCount);
	info.pushConstantRanges = std::vector<VkPushConstantRange>(pCreateInfo->pPushConstantRanges, pCreateInfo->pPushConstantRanges+pCreateInfo->pushConstantRangeCount);

	return result;
}

VkResult device::CreateGraphicsPipelines(VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines)
{
	std::vector<std::remove_cvref_t<decltype(std::declval<rules::local_context>().creationCallbacks)>> callbacks;
	for(unsigned int i=0; i<createInfoCount; i++) {
		VkGraphicsPipelineCreateInfo* info = const_cast<VkGraphicsPipelineCreateInfo*>(&pCreateInfos[i]);

		std::vector<VkHandle> shaderStages;
		std::transform(info->pStages, info->pStages+info->stageCount, std::back_inserter(shaderStages), [this](VkPipelineShaderStageCreateInfo s){
			return customShaderHandles[s.module];
		});

		rules::pipeline_info pinfo = {shaderStages, info};
		rules::calling_context ctx{
			.info = pinfo
		};
		execute_rules(rules::selector_type::Pipeline, VK_NULL_HANDLE, ctx);

		for(const auto& o : ctx.overrides) {
			try {
				reflection::parse_assign(o, info, "VkGraphicsPipelineCreateInfo");
			} catch(const std::exception& e) {
				logger->error("Failed to process override \"{}\": {}", o, e.what());
			}
		}
		callbacks.push_back(ctx.creationCallbacks);
	}
	VkResult result = dispatch.CreateGraphicsPipelines(handle, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);

	if(result != VK_SUCCESS)
		return result;

	for(unsigned int i=0; i<createInfoCount; i++) {
		if(pPipelines[i] == VK_NULL_HANDLE)
			continue;

		pipeline_state state = {};
		VkGraphicsPipelineCreateInfo info = pCreateInfos[i];

		state.stages.resize(info.stageCount);
		for(unsigned int j=0; j<info.stageCount; j++) {
			VkPipelineShaderStageCreateInfo shaderInfo = info.pStages[j];
			VkHandle customHandle = customShaderHandles[shaderInfo.module];

			auto p = inst->global_context.hashes.find(customHandle);
			std::string hash = "unknown";
			if(p != inst->global_context.hashes.end())
				hash = p->second;

			state.stages[j] = {shaderInfo.stage, shaderInfo.module, customHandle, hash, std::string(shaderInfo.pName)};
		}
		state.vertexBindingDescriptions = std::vector(info.pVertexInputState->pVertexBindingDescriptions,
			info.pVertexInputState->pVertexBindingDescriptions + info.pVertexInputState->vertexBindingDescriptionCount);
		state.vertexAttributeDescriptions = std::vector(info.pVertexInputState->pVertexAttributeDescriptions,
			info.pVertexInputState->pVertexAttributeDescriptions + info.pVertexInputState->vertexAttributeDescriptionCount);

		pipelineStates[pPipelines[i]] = std::move(state);

		for(auto& cb : callbacks[i]) {
			cb(pPipelines[i]);
		}
	}
	return result;
}

}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_AllocateCommandBuffers(
    VkDevice                                    device,
    const VkCommandBufferAllocateInfo*          pAllocateInfo,
    VkCommandBuffer*                            pCommandBuffers)
{
	return CheekyLayer::get_device(device).AllocateCommandBuffers(pAllocateInfo, pCommandBuffers);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_FreeCommandBuffers(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    uint32_t                                    commandBufferCount,
    const VkCommandBuffer*                      pCommandBuffers)
{
	return CheekyLayer::get_device(device).FreeCommandBuffers(commandPool, commandBufferCount, pCommandBuffers);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateFramebuffer(
    VkDevice                                    device,
    const VkFramebufferCreateInfo*              pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkFramebuffer*                              pFramebuffer)
{
	return CheekyLayer::get_device(device).CreateFramebuffer(pCreateInfo, pAllocator, pFramebuffer);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreatePipelineLayout(
    VkDevice                                    device,
    const VkPipelineLayoutCreateInfo*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkPipelineLayout*                           pPipelineLayout)
{
	return CheekyLayer::get_device(device).CreatePipelineLayout(pCreateInfo, pAllocator, pPipelineLayout);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateGraphicsPipelines(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkGraphicsPipelineCreateInfo*         pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
	return CheekyLayer::get_device(device).CreateGraphicsPipelines(pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindDescriptorSets(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    firstSet,
    uint32_t                                    descriptorSetCount,
    const VkDescriptorSet*                      pDescriptorSets,
    uint32_t                                    dynamicOffsetCount,
    const uint32_t*                             pDynamicOffsets)
{
	if(evalRulesInDraw)
	{
		CommandBufferState& state = commandBufferStates[commandBuffer];

		if(state.descriptorSets.size() < (firstSet + descriptorSetCount))
			state.descriptorSets.resize(firstSet + descriptorSetCount);
		std::copy(pDescriptorSets, pDescriptorSets+descriptorSetCount, state.descriptorSets.begin() + firstSet);

		if(dynamicOffsetCount && pDynamicOffsets)
		{
			// TODO: support multiple descriptor sets, eeeeeeeh
			if(state.descriptorDynamicOffsets.size() < dynamicOffsetCount)
				state.descriptorDynamicOffsets.resize(dynamicOffsetCount);
			std::copy(pDynamicOffsets, pDynamicOffsets + dynamicOffsetCount, state.descriptorDynamicOffsets.begin());
		}
	}

	quick_dispatch.CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet,
		descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindPipeline(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipeline                                  pipeline)
{
	if(evalRulesInDraw)
	{
		CommandBufferState& state = commandBufferStates[commandBuffer];
		state.pipeline = pipeline;
	}

	quick_dispatch.CmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindVertexBuffers(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets)
{
	if(evalRulesInDraw)
	{
		CommandBufferState& state = commandBufferStates[commandBuffer];

		if(state.vertexBuffers.size() < (firstBinding + bindingCount))
			state.vertexBuffers.resize(firstBinding + bindingCount);
		if(state.vertexBufferOffsets.size() < (firstBinding + bindingCount))
			state.vertexBufferOffsets.resize(firstBinding + bindingCount);

		std::copy(pBuffers, pBuffers+bindingCount, state.vertexBuffers.begin() + firstBinding);
		std::copy(pOffsets, pOffsets+bindingCount, state.vertexBufferOffsets.begin() + firstBinding);
	}

	quick_dispatch.CmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindVertexBuffers2EXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets,
    const VkDeviceSize*                         pSizes,
    const VkDeviceSize*                         pStrides)
{
	if(evalRulesInDraw)
	{
		CommandBufferState& state = commandBufferStates[commandBuffer];

		if(state.vertexBuffers.size() < (firstBinding + bindingCount))
			state.vertexBuffers.resize(firstBinding + bindingCount);
		if(state.vertexBufferOffsets.size() < (firstBinding + bindingCount))
			state.vertexBufferOffsets.resize(firstBinding + bindingCount);

		std::copy(pBuffers, pBuffers+bindingCount, state.vertexBuffers.begin() + firstBinding);
		std::copy(pOffsets, pOffsets+bindingCount, state.vertexBufferOffsets.begin() + firstBinding);
	}

	quick_dispatch.CmdBindVertexBuffers2EXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindIndexBuffer(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType)
{
	if(evalRulesInDraw)
	{
		CommandBufferState& state = commandBufferStates[commandBuffer];

		state.indexBuffer = buffer;
		state.indexBufferOffset = offset;
		state.indexType = indexType;
	}

	quick_dispatch.CmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdSetScissor(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstScissor,
    uint32_t                                    scissorCount,
    const VkRect2D*                             pScissors)
{
	CommandBufferState& state = commandBufferStates[commandBuffer];

	if(state.scissors.size() < (firstScissor + scissorCount))
		state.scissors.resize(firstScissor + scissorCount);
	std::copy(pScissors, pScissors+scissorCount, state.scissors.begin() + firstScissor);

	quick_dispatch.CmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBeginRenderPass(
    VkCommandBuffer                             commandBuffer,
    const VkRenderPassBeginInfo*                pRenderPassBegin,
    VkSubpassContents                           contents)
{
	CommandBufferState& state = commandBufferStates[commandBuffer];
	state.renderpass = pRenderPassBegin->renderPass;
	state.framebuffer = pRenderPassBegin->framebuffer;

	quick_dispatch.CmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdEndRenderPass(
    VkCommandBuffer                             commandBuffer)
{
	if(evalRulesInDraw)
	{
		CommandBufferState& state = commandBufferStates[commandBuffer];

		scoped_lock l(global_lock);
		auto p = CheekyLayer::rules::rule_env.on_EndRenderPass.find(commandBuffer);
		if(p != CheekyLayer::rules::rule_env.on_EndRenderPass.end() && !p->second.empty())
		{
			CheekyLayer::active_logger log = *logger << logger::begin;
			CheekyLayer::rules::local_context ctx = {.logger = log, .commandBuffer = commandBuffer, .device = state.device, .commandBufferState = &state };

			for(auto& f : p->second)
			{
				try
				{
					f(ctx);
				}
				catch(const std::exception& ex)
				{
					ctx.logger << logger::error << "Failed to execute a callback: " << ex.what();
				}
			}
			p->second.clear();

			log << logger::end;
		}
	}

	quick_dispatch.CmdEndRenderPass(commandBuffer);
}

void verbose_pipeline_stages(CheekyLayer::active_logger& log, PipelineState& pstate)
{
	log << "  pipeline stages:\n";
	log << "    |    stage |       name | shader\n";
	for(auto shader : pstate.stages)
	{
		log << "    | " << std::setw(8) << vk::to_string((vk::ShaderStageFlagBits)shader.stage) << " | ";
		log << std::setw(10) << shader.name;
		log << " | " << shader.hash;
		log << '\n';
	}
}

void verbose_vertex_bindings(CheekyLayer::active_logger& log, CommandBufferState& state, PipelineState& pstate, bool index)
{
	log << "  vertex bindings:\n";
	log << "    | binding | stride |     rate | buffer\n";
	if(index)
	{
		log << "    | " << std::setw(7) << "index" << " | ";
		log << std::setw(6);
		switch(state.indexType)
		{
			case VK_INDEX_TYPE_UINT16:
				log << 2;
				break;
			case VK_INDEX_TYPE_UINT32:
				log << 4;
				break;
			default:
				log << vk::to_string((vk::IndexType)state.indexType);
		}
		log << " | " << std::setw(8) << "-";

		auto p = CheekyLayer::rules::rule_env.hashes.find((VkHandle)state.indexBuffer);
		if(p != CheekyLayer::rules::rule_env.hashes.end())
		{
			log << " | " << p->second;
		}
		else
		{
			log << " | " << "unknown buffer " << state.indexBuffer;
		}
		log << " + " << std::hex << state.indexBufferOffset << std::dec;
		log << '\n';
	}
	for(auto b : pstate.vertexBindingDescriptions)
	{
		log << "    | "
			<< std::setw(7) << b.binding   << " | "
			<< std::setw(6) << b.stride    << " | "
			<< std::setw(8) << vk::to_string((vk::VertexInputRate)b.inputRate);

		if(state.vertexBuffers.size() > b.binding)
		{
			VkBuffer buffer = state.vertexBuffers[b.binding];
			auto p = CheekyLayer::rules::rule_env.hashes.find((VkHandle)buffer);
			if(p != CheekyLayer::rules::rule_env.hashes.end())
			{
				log << " | " << p->second;
			}
			else
			{
				log << " | " << "unknown buffer " << buffer;
			}
			log << " + " << std::hex << state.vertexBufferOffsets[b.binding] << std::dec;
		}
		log << '\n';
	}
}

void verbose_vertex_attributes(CheekyLayer::active_logger& log, PipelineState& pstate)
{
	log << "  vertex attributes:\n";
	log << "    | binding | location | offset | format\n";
	for(auto a : pstate.vertexAttributeDescriptions)
	{
		log << "    | "
			<< std::setw(7) << a.binding  << " | "
			<< std::setw(8) << a.location << " | "
			<< std::setw(6) << a.offset   << " | "
			<< std::setw(6) << vk::to_string((vk::Format)a.format)   << '\n';
	}
}

void verbose_descriptors(CheekyLayer::active_logger& log, CommandBufferState& state)
{
	log << "  descriptors:\n";
	log << "    | set | binding |   type |                exact type |         offset | elements\n";
	for(int i=0; i<state.descriptorSets.size(); i++)
	{
		VkDescriptorSet set = state.descriptorSets[i];
		if(descriptorStates.contains(set))
		{
			DescriptorState& descriptorState = descriptorStates[set];
			for(auto& [binding, info] : descriptorState.bindings)
			{
				log << "    | "
					<< std::setw(3) << i << " | "
					<< std::setw(7) << binding << " | "
					<< std::setw(6) << to_string(info.type) << " | "
					<< std::setw(25) << vk::to_string((vk::DescriptorType)info.exactType) << " | ";
				if(state.descriptorDynamicOffsets.size() > binding)
					log << std::setw(14) << state.descriptorDynamicOffsets[binding];
				else
					log << "unknown offset";
				log << " | ";
				std::transform(info.arrayElements.begin(), info.arrayElements.end(), std::experimental::make_ostream_joiner(log.raw(), ", "), [](auto a){
					return a.handle;
				});
				log << '\n';
			}
		}
		else
		{
			log << "    | " << std::setw(3) << i << " | unrecognized descriptor set " << set << '\n';
		}
	}
}

void verbose_commandbuffer_state(CheekyLayer::active_logger& log, CommandBufferState& state)
{
	log << "  command buffer state:\n";
	log << "    transformFeedback = " << std::boolalpha << state.transformFeedback << '\n';
	if(state.transformFeedback)
	{
		log << "    transformFeedbackBuffers:\n";
		for(const auto& binding : state.transformFeedbackBuffers)
		{
			log << "      ";
			VkBuffer buffer = binding.buffer;
			auto p = CheekyLayer::rules::rule_env.hashes.find((VkHandle)buffer);
			if(p != CheekyLayer::rules::rule_env.hashes.end())
			{
				log << p->second;
			}
			else
			{
				log << "unknown buffer " << buffer;
			}
			log << " + " << std::hex << binding.offset << std::dec << " (" << binding.size << " bytes)\n";
		}
	}
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdDrawIndexed(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    indexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstIndex,
    int32_t                                     vertexOffset,
    uint32_t                                    firstInstance)
{
	// quick exit to make things faster here
	if(!evalRulesInDraw)
	{
		quick_dispatch.CmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
		return;
	}
	CommandBufferState& state = commandBufferStates[commandBuffer];

	std::vector<VkImage> images;
	std::vector<VkBuffer> buffers;
	std::vector<VkHandle> shaders;
	for(auto d : state.descriptorSets)
	{
		if(d)
		{
			DescriptorState& ds = descriptorStates[d];
			for(auto it = ds.bindings.begin(); it != ds.bindings.end(); it++)
			{
				DescriptorBinding& binding = it->second;
				if(binding.type == CheekyLayer::rules::selector_type::Image)
				{
					std::transform(binding.arrayElements.begin(), binding.arrayElements.end(), std::back_inserter(images), [](auto h){
						return (VkImage) h.handle;
					});
				}
			}
		}
	}
	PipelineState& pstate = pipelineStates[state.pipeline];
	std::transform(pstate.stages.begin(), pstate.stages.end(), std::back_inserter(shaders), [](ShaderInfo s){
		return s.customHandle;
	});
	buffers.push_back(state.indexBuffer);
	std::copy(state.vertexBuffers.begin(), state.vertexBuffers.end(), std::back_inserter(buffers));

	CheekyLayer::reflection::VkCmdDrawIndexed drawCall = {
		.indexCount = indexCount,
		.instanceCount = instanceCount,
		.firstIndex = firstIndex,
		.vertexOffset = vertexOffset,
		.firstInstance = firstInstance,
		.commandBufferState = CheekyLayer::reflection::VkCommandBufferState{
			.scissorCount = static_cast<uint32_t>(state.scissors.size()),
			.pScissors = state.scissors.data(),
			.transformFeedback = state.transformFeedback
		}
	};
	CheekyLayer::rules::draw_info info = {images, shaders, buffers, state.indexBuffer, &drawCall};
	CheekyLayer::rules::additional_info info2 = {info};

	CheekyLayer::active_logger log = *logger << logger::begin;

	CheekyLayer::rules::local_context ctx = {log, [&state, &commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance](CheekyLayer::active_logger log){
		log << "CmdDrawIndexed: on device " << state.device << " from command buffer " << commandBuffer << " with pipeline " << state.pipeline << '\n';

		log << std::dec << std::setfill(' ');

		log << "  draw parameters:\n";
		log << "    indexCount = " << indexCount << '\n';
		log << "    instanceCount = " << instanceCount << '\n';
		log << "    firstIndex = " << firstIndex << '\n';
		log << "    vertexOffset = " << vertexOffset << '\n';
		log << "    firstInstance = " << firstInstance << '\n';

		PipelineState& pstate = pipelineStates[state.pipeline];
		verbose_commandbuffer_state(log, state);
		verbose_pipeline_stages(log, pstate);
		verbose_vertex_bindings(log, state, pstate, true);
		verbose_vertex_attributes(log, pstate);
		verbose_descriptors(log, state);
	}, &info2, commandBuffer, state.device, {}, {}, &state};
	CheekyLayer::rules::execute_rules(rules, CheekyLayer::rules::selector_type::Draw, VK_NULL_HANDLE, ctx);

	log << logger::end;

	if(!ctx.canceled)
		quick_dispatch.CmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdDraw(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    vertexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstVertex,
    uint32_t                                    firstInstance)
{
	// quick exit to make things faster here
	if(!evalRulesInDraw)
	{
		quick_dispatch.CmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
		return;
	}
	CommandBufferState& state = commandBufferStates[commandBuffer];

	std::vector<VkImage> images;
	std::vector<VkBuffer> buffers;
	std::vector<VkHandle> shaders;
	for(auto d : state.descriptorSets)
	{
		if(d)
		{
			DescriptorState& ds = descriptorStates[d];
			for(auto it = ds.bindings.begin(); it != ds.bindings.end(); it++)
			{
				DescriptorBinding& binding = it->second;
				if(binding.type == CheekyLayer::rules::selector_type::Image)
				{
					std::transform(binding.arrayElements.begin(), binding.arrayElements.end(), std::back_inserter(images), [](auto h){
						return (VkImage) h.handle;
					});
				}
			}
		}
	}
	PipelineState& pstate = pipelineStates[state.pipeline];
	std::transform(pstate.stages.begin(), pstate.stages.end(), std::back_inserter(shaders), [](ShaderInfo s){
		return s.customHandle;
	});
	std::copy(state.vertexBuffers.begin(), state.vertexBuffers.end(), std::back_inserter(buffers));

	CheekyLayer::reflection::VkCmdDraw drawCall = {
		.vertexCount = vertexCount,
		.instanceCount = instanceCount,
		.firstVertex = firstVertex,
		.firstInstance = firstInstance,
		.commandBufferState = CheekyLayer::reflection::VkCommandBufferState{
			.scissorCount = static_cast<uint32_t>(state.scissors.size()),
			.pScissors = state.scissors.data(),
			.transformFeedback = state.transformFeedback
		}
	};
	CheekyLayer::rules::draw_info info = {images, shaders, buffers, state.indexBuffer, &drawCall};
	CheekyLayer::rules::additional_info info2 = {info};

	CheekyLayer::active_logger log = *logger << logger::begin;

	CheekyLayer::rules::local_context ctx = {log, [&state, &commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance](CheekyLayer::active_logger log){
		log << "CmdDraw: on device " << state.device << " from command buffer " << commandBuffer << " with pipeline " << state.pipeline << '\n';

		log << std::dec << std::setfill(' ');

		log << "  draw parameters:\n";
		log << "    vertexCount = " << vertexCount << '\n';
		log << "    instanceCount = " << instanceCount << '\n';
		log << "    firstVertex = " << firstVertex << '\n';
		log << "    firstInstance = " << firstInstance << '\n';

		PipelineState& pstate = pipelineStates[state.pipeline];
		verbose_commandbuffer_state(log, state);
		verbose_pipeline_stages(log, pstate);
		verbose_vertex_bindings(log, state, pstate, false);
		verbose_vertex_attributes(log, pstate);
		verbose_descriptors(log, state);
	}, &info2, commandBuffer, state.device, {}, {}, &state};
	CheekyLayer::rules::execute_rules(rules, CheekyLayer::rules::selector_type::Draw, VK_NULL_HANDLE, ctx);

	log << logger::end;

	if(!ctx.canceled)
		quick_dispatch.CmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBeginTransformFeedbackEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstCounterBuffer,
    uint32_t                                    counterBufferCount,
    const VkBuffer*                             pCounterBuffers,
    const VkDeviceSize*                         pCounterBufferOffsets)
{
	CommandBufferState& state = commandBufferStates[commandBuffer];
	state.transformFeedback = true;
	quick_dispatch.CmdBeginTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindTransformFeedbackBuffersEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets,
    const VkDeviceSize*                         pSizes)
{
	if(evalRulesInDraw)
	{
		CommandBufferState& state = commandBufferStates[commandBuffer];

		if(state.transformFeedbackBuffers.size() < (firstBinding + bindingCount))
			state.transformFeedbackBuffers.resize(firstBinding + bindingCount);

		for(int i=0; i<bindingCount; i++)
		{
			state.transformFeedbackBuffers[i+firstBinding] = {.buffer = pBuffers[i], .offset = pOffsets[i], .size = pSizes[i]};
		}
	}

	quick_dispatch.CmdBindTransformFeedbackBuffersEXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdEndTransformFeedbackEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstCounterBuffer,
    uint32_t                                    counterBufferCount,
    const VkBuffer*                             pCounterBuffers,
    const VkDeviceSize*                         pCounterBufferOffsets)
{
	CommandBufferState& state = commandBufferStates[commandBuffer];
	state.transformFeedback = false;
	state.transformFeedbackBuffers.clear();
	quick_dispatch.CmdEndTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EndCommandBuffer(
    VkCommandBuffer                             commandBuffer)
{
	if(evalRulesInDraw)
	{
		CommandBufferState& state = commandBufferStates[commandBuffer];
		state.transformFeedback = false;

		auto p = CheekyLayer::rules::rule_env.on_EndCommandBuffer.find(commandBuffer);
		if(p != CheekyLayer::rules::rule_env.on_EndCommandBuffer.end() && !p->second.empty())
		{
			CheekyLayer::active_logger log = *logger << logger::begin;
				CheekyLayer::rules::local_context ctx = {.logger = log, .device = state.device};

			for(auto& f : p->second)
			{
				try
				{
					f(ctx);
				}
				catch(const std::exception& ex)
				{
					ctx.logger << logger::error << "Failed to execute a callback: " << ex.what();
				}
			}
			p->second.clear();

			log << logger::end;
		}
	}

	return quick_dispatch.EndCommandBuffer(commandBuffer);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateSwapchainKHR(
    VkDevice                                    device,
    const VkSwapchainCreateInfoKHR*             pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSwapchainKHR*                             pSwapchain)
{
	return CheekyLayer::get_device(device).CreateSwapchainKHR(pCreateInfo, pAllocator, pSwapchain);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_QueuePresentKHR(
    VkQueue                                     queue,
    const VkPresentInfoKHR*                     pPresentInfo)
{
	VkDevice device = queueDevices[queue];

	CheekyLayer::rules::present_info info = {pPresentInfo};
	CheekyLayer::rules::additional_info info2 = {.present = info};

	CheekyLayer::active_logger log = *logger << logger::begin;
	CheekyLayer::rules::local_context ctx = { .logger = log, .info = &info2, .device = device, .canceled = false };
	CheekyLayer::rules::execute_rules(rules, CheekyLayer::rules::selector_type::Present, (VkHandle) queue, ctx);
	log << logger::end;

	if(!ctx.canceled)
		return device_dispatch[GetKey(device)].QueuePresentKHR(queue, pPresentInfo);
	return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_QueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    submitCount,
    const VkSubmitInfo*                         pSubmits,
    VkFence                                     fence)
{
	VkDevice device = queueDevices[queue];
	for(int i=0; i<submitCount; i++)
	{
		const auto& submit = pSubmits[i];
		for(int j=0; j<submit.commandBufferCount; j++)
		{
			auto commandBuffer = submit.pCommandBuffers[j];
			auto p = CheekyLayer::rules::rule_env.on_QueueSubmit.find(commandBuffer);
			if(p != CheekyLayer::rules::rule_env.on_QueueSubmit.end() && !p->second.empty())
			{
				CheekyLayer::active_logger log = *logger << logger::begin;
				CheekyLayer::rules::local_context ctx = {.logger = log, .device = device};

				for(auto& f : p->second)
				{
					try
					{
						f(ctx);
					}
					catch(const std::exception& ex)
					{
						ctx.logger << logger::error << "Failed to execute a callback: " << ex.what();
					}
				}
				p->second.clear();

				log << logger::end;
			}
		}
	}
	return device_dispatch[GetKey(device)].QueueSubmit(queue, submitCount, pSubmits, fence);
}
