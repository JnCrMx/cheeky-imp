#include "draw.hpp"
#include "dispatch.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "descriptors.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include <iomanip>
#include <iterator>
#include <vulkan/vulkan_core.h>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

using CheekyLayer::logger;
using CheekyLayer::VkHandle;

std::map<VkCommandBuffer, CommandBufferState> commandBufferStates;
std::map<VkPipeline, PipelineState> pipelineStates;

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_AllocateCommandBuffers(
    VkDevice                                    device,
    const VkCommandBufferAllocateInfo*          pAllocateInfo,
    VkCommandBuffer*                            pCommandBuffers)
{
	VkResult result = device_dispatch[GetKey(device)].AllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
	CheekyLayer::active_logger log = *logger << logger::begin << "AllocateCommandBuffers: " << pAllocateInfo->commandBufferCount << " -> {";
	for(int i=0; i<pAllocateInfo->commandBufferCount; i++)
	{
		log << pCommandBuffers[i] << (i==pAllocateInfo->commandBufferCount-1?"}":",");
		commandBufferStates[pCommandBuffers[i]] = {device};

		CheekyLayer::rule_env.on_EndCommandBuffer[pCommandBuffers[i]] = {};
		CheekyLayer::rule_env.on_QueueSubmit[pCommandBuffers[i]] = {};
	}
	log << logger::end;
	return result;
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_FreeCommandBuffers(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    uint32_t                                    commandBufferCount,
    const VkCommandBuffer*                      pCommandBuffers)
{
	CheekyLayer::active_logger log = *logger << logger::begin << "FreeCommandBuffers: " << commandBufferCount << " -> {";
	for(int i=0; i<commandBufferCount; i++)
	{
		log << pCommandBuffers[i] << (i==commandBufferCount-1?"}":",");
		commandBufferStates.erase(pCommandBuffers[i]);

		CheekyLayer::rule_env.on_EndCommandBuffer.erase(pCommandBuffers[i]);
		CheekyLayer::rule_env.on_QueueSubmit.erase(pCommandBuffers[i]);
	}
	log << logger::end;
	device_dispatch[GetKey(device)].FreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateGraphicsPipelines(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkGraphicsPipelineCreateInfo*         pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
    VkResult result = device_dispatch[GetKey(device)].CreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);

	if(result == VK_SUCCESS)
	{
		for(int i=0; i<createInfoCount; i++)
		{
			if(pPipelines[i] == VK_NULL_HANDLE)
				continue;

			PipelineState state = {};
			VkGraphicsPipelineCreateInfo info = pCreateInfos[i];

			state.stages.resize(info.stageCount);
			for(int j=0; j<info.stageCount; j++)
			{
				VkPipelineShaderStageCreateInfo shaderInfo = info.pStages[j];

				auto p = CheekyLayer::rule_env.hashes.find(shaderInfo.module);
				std::string hash = "unknown";
				if(p != CheekyLayer::rule_env.hashes.end())
					hash = p->second;

				state.stages[j] = {shaderInfo.stage, shaderInfo.module, hash, std::string(shaderInfo.pName)};
			}
			state.vertexBindingDescriptions = std::vector(info.pVertexInputState->pVertexBindingDescriptions,
				info.pVertexInputState->pVertexBindingDescriptions + info.pVertexInputState->vertexBindingDescriptionCount);
			state.vertexAttributeDescriptions = std::vector(info.pVertexInputState->pVertexAttributeDescriptions,
				info.pVertexInputState->pVertexAttributeDescriptions + info.pVertexInputState->vertexAttributeDescriptionCount);

			pipelineStates[pPipelines[i]] = std::move(state);
		}
	}

    return result;
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
	CommandBufferState& state = commandBufferStates[commandBuffer];

	if(state.descriptorSets.size() < firstSet + descriptorSetCount)
		state.descriptorSets.resize(firstSet + descriptorSetCount);
	for(int i=0; i<descriptorSetCount; i++)
	{
		state.descriptorSets[firstSet + i] = pDescriptorSets[i];
	}

	device_dispatch[GetKey(state.device)].CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet,
		descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindPipeline(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipeline                                  pipeline)
{
	CommandBufferState& state = commandBufferStates[commandBuffer];
	state.pipeline = pipeline;

	device_dispatch[GetKey(state.device)].CmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindVertexBuffers(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets)
{
	CommandBufferState& state = commandBufferStates[commandBuffer];

	if(state.vertexBuffers.size() < firstBinding + bindingCount)
		state.vertexBuffers.resize(firstBinding + bindingCount);
	if(state.vertexBufferOffsets.size() < firstBinding + bindingCount)
		state.vertexBufferOffsets.resize(firstBinding + bindingCount);

	std::copy(pBuffers, pBuffers+bindingCount, state.vertexBuffers.begin() + firstBinding);
	std::copy(pOffsets, pOffsets+bindingCount, state.vertexBufferOffsets.begin() + firstBinding);

	device_dispatch[GetKey(state.device)].CmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
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
	CommandBufferState& state = commandBufferStates[commandBuffer];

	if(state.vertexBuffers.size() < firstBinding + bindingCount)
		state.vertexBuffers.resize(firstBinding + bindingCount);
	if(state.vertexBufferOffsets.size() < firstBinding + bindingCount)
		state.vertexBufferOffsets.resize(firstBinding + bindingCount);

	std::copy(pBuffers, pBuffers+bindingCount, state.vertexBuffers.begin() + firstBinding);
	std::copy(pOffsets, pOffsets+bindingCount, state.vertexBufferOffsets.begin() + firstBinding);

	device_dispatch[GetKey(state.device)].CmdBindVertexBuffers2EXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindIndexBuffer(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType)
{
	CommandBufferState& state = commandBufferStates[commandBuffer];

	state.indexBuffer = buffer;
	state.indexBufferOffset = offset;
	state.indexType = indexType;

	device_dispatch[GetKey(state.device)].CmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdDrawIndexed(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    indexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstIndex,
    int32_t                                     vertexOffset,
    uint32_t                                    firstInstance)
{
	CommandBufferState& state = commandBufferStates[commandBuffer];

	// quick exit to make things faster here
	if(!has_rules[CheekyLayer::selector_type::Draw])
	{
		device_dispatch[GetKey(state.device)].CmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
		return;
	}

	std::vector<VkImage> images;
	std::vector<VkBuffer> buffers;
	std::vector<VkShaderModule> shaders;
	for(auto d : state.descriptorSets)
	{
		if(d)
		{
			DescriptorState& ds = descriptorStates[d];
			for(auto it = ds.bindings.begin(); it != ds.bindings.end(); it++)
			{
				DescriptorBinding& binding = it->second;
				if(binding.type == CheekyLayer::selector_type::Image)
				{
					std::transform(binding.arrayElements.begin(), binding.arrayElements.end(), std::back_inserter(images), [](auto h){
						return (VkImage) h;
					});
				}
			}
		}
	}
	CheekyLayer::draw_info info = {images, shaders, buffers, VK_NULL_HANDLE};
	CheekyLayer::additional_info info2 = {info};

	CheekyLayer::active_logger log = *logger << logger::begin;

	CheekyLayer::local_context ctx = {log, [&state, &commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance](CheekyLayer::active_logger log){
		log << "CmdDraw: on device " << state.device << " from command buffer " << commandBuffer << " with pipeline " << state.pipeline << '\n';

		log << std::dec << std::setfill(' ');

		log << "  draw parameters:\n";
		log << "    indexCount = " << indexCount << '\n';
		log << "    instanceCount = " << instanceCount << '\n';
		log << "    firstIndex = " << firstIndex << '\n';
		log << "    vertexOffset = " << vertexOffset << '\n';
		log << "    firstInstance = " << firstInstance << '\n';

		PipelineState& pstate = pipelineStates[state.pipeline];
		log << "  pipeline stages:\n";
		log << "    |    stage |       name | shader\n";
		for(auto shader : pstate.stages)
		{
			log << "    | " << std::setw(8) << vk::to_string((vk::ShaderStageFlagBits)shader.stage) << " | ";
			log << std::setw(10) << shader.name;
			log << " | " << shader.hash;
			log << '\n';
		}

		log << "  vertex bindings:\n";
		log << "    | binding | stride |     rate | buffer\n";
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

			auto p = CheekyLayer::rule_env.hashes.find((VkHandle)state.indexBuffer);
			if(p != CheekyLayer::rule_env.hashes.end())
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
				auto p = CheekyLayer::rule_env.hashes.find((VkHandle)buffer);
				if(p != CheekyLayer::rule_env.hashes.end())
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
	}, &info2, commandBuffer};
	CheekyLayer::execute_rules(rules, CheekyLayer::selector_type::Draw, VK_NULL_HANDLE, ctx);

	log << logger::end;

	if(!ctx.canceled)
		device_dispatch[GetKey(state.device)].CmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EndCommandBuffer(
    VkCommandBuffer                             commandBuffer)
{
	CommandBufferState& state = commandBufferStates[commandBuffer];

	auto p = CheekyLayer::rule_env.on_EndCommandBuffer.find(commandBuffer);
	if(p != CheekyLayer::rule_env.on_EndCommandBuffer.end())
	{
		CheekyLayer::active_logger log = *logger << logger::begin;
		CheekyLayer::local_context ctx = {log};

		for(auto& f : p->second)
		{
			f(ctx);
		}
		p->second.clear();

		log << logger::end;
	}

	return device_dispatch[GetKey(state.device)].EndCommandBuffer(commandBuffer);
}
