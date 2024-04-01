#include "objects.hpp"
#include "layer.hpp"

#include "reflection/reflectionparser.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"

#include <experimental/iterator>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

std::string verbose_pipeline_stages(CheekyLayer::pipeline_state& pstate)
{
	std::ostringstream log;
	log << "  pipeline stages:\n";
	log << "    |    stage |       name | shader\n";
	for(auto shader : pstate.stages)
	{
		log << "    | " << std::setw(8) << vk::to_string((vk::ShaderStageFlagBits)shader.stage) << " | ";
		log << std::setw(10) << shader.name;
		log << " | " << shader.hash;
		log << '\n';
	}
	return log.str();
}

std::string verbose_vertex_bindings(CheekyLayer::device& device, CheekyLayer::command_buffer_state& state, CheekyLayer::pipeline_state& pstate, bool index)
{
	std::ostringstream log;
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

		auto p = device.inst->global_context.hashes.find((CheekyLayer::rules::VkHandle)state.indexBuffer);
		if(p != device.inst->global_context.hashes.end())
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
			auto p = device.inst->global_context.hashes.find((CheekyLayer::rules::VkHandle)buffer);
			if(p != device.inst->global_context.hashes.end())
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
	return log.str();
}

std::string verbose_vertex_attributes(CheekyLayer::device&, CheekyLayer::pipeline_state& pstate)
{
	std::ostringstream log;
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
	return log.str();
}

std::string verbose_descriptors(CheekyLayer::device& device, CheekyLayer::command_buffer_state& state)
{
	std::ostringstream log;
	log << "  descriptors:\n";
	log << "    | set | binding |   type |                exact type |         offset | elements\n";
	for(int i=0; i<state.descriptorSets.size(); i++)
	{
		VkDescriptorSet set = state.descriptorSets[i];
		if(device.descriptorStates.contains(set))
		{
			auto& descriptorState = device.descriptorStates[set];
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
				std::transform(info.arrayElements.begin(), info.arrayElements.end(), std::experimental::make_ostream_joiner(log, ", "), [](auto a){
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
	return log.str();
}

std::string verbose_commandbuffer_state(CheekyLayer::device& device, CheekyLayer::command_buffer_state& state)
{
	std::ostringstream log;
	log << "  command buffer state:\n";
	log << "    transformFeedback = " << std::boolalpha << state.transformFeedback << '\n';
	if(state.transformFeedback)
	{
		log << "    transformFeedbackBuffers:\n";
		for(const auto& binding : state.transformFeedbackBuffers)
		{
			log << "      ";
			VkBuffer buffer = binding.buffer;
			auto p = device.inst->global_context.hashes.find((CheekyLayer::rules::VkHandle)buffer);
			if(p != device.inst->global_context.hashes.end())
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
	return log.str();
}

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
	execute_rules(rules::selector_type::SwapchainCreate, (rules::VkHandle)(*pSwapchain), ctx);

	return result;
}

VkResult device::AllocateCommandBuffers(const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) {
	VkResult result = dispatch.AllocateCommandBuffers(handle, pAllocateInfo, pCommandBuffers);
	if(result != VK_SUCCESS)
		return result;

	logger->debug("AllocateCommandBuffers: {}", pAllocateInfo->commandBufferCount);

	for(int i=0; i<pAllocateInfo->commandBufferCount; i++)
	{
		commandBufferStates[pCommandBuffers[i]] = {handle};

		inst->global_context.on_EndCommandBuffer[pCommandBuffers[i]] = {};
		inst->global_context.on_QueueSubmit[pCommandBuffers[i]] = {};
	}

	return result;
}

void device::FreeCommandBuffers(VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
	logger->debug("FreeCommandBuffers: {}", commandBufferCount);

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

		std::vector<rules::VkHandle> shaderStages;
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
			rules::VkHandle customHandle = customShaderHandles[shaderInfo.module];

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

void device::CmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets)
{
	auto& state = commandBufferStates[commandBuffer];
	if(state.descriptorSets.size() < (firstSet + descriptorSetCount))
		state.descriptorSets.resize(firstSet + descriptorSetCount);
	std::copy(pDescriptorSets, pDescriptorSets+descriptorSetCount, state.descriptorSets.begin() + firstSet);

	if(dynamicOffsetCount && pDynamicOffsets) {
		// TODO: support multiple descriptor sets, eeeeeeeh
		if(state.descriptorDynamicOffsets.size() < dynamicOffsetCount)
			state.descriptorDynamicOffsets.resize(dynamicOffsetCount);
		std::copy(pDynamicOffsets, pDynamicOffsets + dynamicOffsetCount, state.descriptorDynamicOffsets.begin());
	}

	dispatch.CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

void device::CmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
	auto& state = commandBufferStates[commandBuffer];
	state.pipeline = pipeline;
	dispatch.CmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

void device::CmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets)
{
	auto& state = commandBufferStates[commandBuffer];
	if(state.vertexBuffers.size() < (firstBinding + bindingCount))
		state.vertexBuffers.resize(firstBinding + bindingCount);
	if(state.vertexBufferOffsets.size() < (firstBinding + bindingCount))
		state.vertexBufferOffsets.resize(firstBinding + bindingCount);

	std::copy(pBuffers, pBuffers+bindingCount, state.vertexBuffers.begin() + firstBinding);
	std::copy(pOffsets, pOffsets+bindingCount, state.vertexBufferOffsets.begin() + firstBinding);

	dispatch.CmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}

void device::CmdBindVertexBuffers2EXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides)
{
	auto& state = commandBufferStates[commandBuffer];
	if(state.vertexBuffers.size() < (firstBinding + bindingCount))
		state.vertexBuffers.resize(firstBinding + bindingCount);
	if(state.vertexBufferOffsets.size() < (firstBinding + bindingCount))
		state.vertexBufferOffsets.resize(firstBinding + bindingCount);

	std::copy(pBuffers, pBuffers+bindingCount, state.vertexBuffers.begin() + firstBinding);
	std::copy(pOffsets, pOffsets+bindingCount, state.vertexBufferOffsets.begin() + firstBinding);

	dispatch.CmdBindVertexBuffers2EXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}

void device::CmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
{
	auto& state = commandBufferStates[commandBuffer];
	state.indexBuffer = buffer;
	state.indexBufferOffset = offset;
	state.indexType = indexType;

	dispatch.CmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}

void device::CmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors)
{
	auto& state = commandBufferStates[commandBuffer];
	if(state.scissors.size() < (firstScissor + scissorCount))
		state.scissors.resize(firstScissor + scissorCount);
	std::copy(pScissors, pScissors+scissorCount, state.scissors.begin() + firstScissor);

	dispatch.CmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
}

void device::CmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents)
{
	logger->trace("CmdBeginRenderPass in commandBuffer {} with renderPass {} and framebuffer {}",
		fmt::ptr(commandBuffer), fmt::ptr(pRenderPassBegin->renderPass), fmt::ptr(pRenderPassBegin->framebuffer));

	auto& state = commandBufferStates[commandBuffer];
	state.renderpass = pRenderPassBegin->renderPass;
	state.framebuffer = pRenderPassBegin->framebuffer;

	dispatch.CmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
}

void device::CmdEndRenderPass(VkCommandBuffer commandBuffer)
{
	auto& state = commandBufferStates[commandBuffer];

	logger->trace("CmdEndRenderPass in commandBuffer {} with former renderPass {} and former framebuffer {}",
		fmt::ptr(commandBuffer), fmt::ptr(state.renderpass), fmt::ptr(state.framebuffer));

	auto& map = inst->global_context.on_EndRenderPass;
	auto p = map.find(commandBuffer);
	if(p != map.end() && !p->second.empty()) {
		bool _1;
		std::vector<std::string> _2;
		std::string _3;
		std::vector<std::function<void(rules::VkHandle)>> _4;
		std::unordered_map<std::string, rules::data_value> _5;
		void* _6;

		rules::local_context ctx = {
			.logger = *logger,
			.instance = inst,
			.device = this,
			.commandBuffer = commandBuffer,
			.commandBufferState = &state,
			.canceled = _1,
			.overrides = _2,
			.customTag = _3,
			.creationCallbacks = _4,
			.local_variables = _5,
			.customPointer = _6
		};

		for(auto& f : p->second) {
			try {
				f(ctx);
			} catch(const std::exception& e) {
				logger->error("Failed to execute callback: {}", e.what());
			}
		}
		p->second.clear();
	}

	dispatch.CmdEndRenderPass(commandBuffer);
	state.renderpass = VK_NULL_HANDLE;
	state.framebuffer = VK_NULL_HANDLE;
}

void device::CmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	auto& state = commandBufferStates[commandBuffer];

	std::vector<VkImage> images;
	std::vector<VkBuffer> buffers;
	std::vector<rules::VkHandle> shaders;
	for(const auto& d : state.descriptorSets) {
		if(!d) {
			continue;
		}

		const auto& ds = descriptorStates[d];
		for(const auto& [_, binding] : ds.bindings) {
			switch(binding.type) {
				case rules::selector_type::Image:
					std::transform(binding.arrayElements.begin(), binding.arrayElements.end(), std::back_inserter(images), [](const auto& h){
						return (VkImage) h.handle;
					});
					break;
				default:
					;
			}
		}
	}

	auto& pstate = pipelineStates[state.pipeline];
	std::transform(pstate.stages.begin(), pstate.stages.end(), std::back_inserter(shaders), [](const auto& s){
		return s.customHandle;
	});
	std::copy(state.vertexBuffers.begin(), state.vertexBuffers.end(), std::back_inserter(buffers));

	reflection::VkCmdDraw drawCall = {
		.vertexCount = vertexCount,
		.instanceCount = instanceCount,
		.firstVertex = firstVertex,
		.firstInstance = firstInstance,
		.commandBufferState = {
			.scissorCount = static_cast<uint32_t>(state.scissors.size()),
			.pScissors = state.scissors.data(),
			.transformFeedback = state.transformFeedback
		}
	};
	rules::draw_info info = {images, shaders, buffers, state.indexBuffer, &drawCall};

	auto printVerbose = [&](spdlog::logger& logger){
		logger.info(
R"(CmdDraw: on device {} from command buffer {} with pipeline {}
  draw parameters:
	vertexCount = {}
	instanceCount = {}
	firstVertex = {}
	firstInstance = {}
{}
{}
{}
{}
{}
		)", fmt::ptr(state.device), fmt::ptr(commandBuffer), fmt::ptr(state.pipeline),
			vertexCount, instanceCount, firstVertex, firstInstance,
			verbose_commandbuffer_state(*this, state),
			verbose_pipeline_stages(pstate),
			verbose_vertex_bindings(*this, state, pstate, false),
			verbose_vertex_attributes(*this, pstate),
			verbose_descriptors(*this, state)
		);
	};

	rules::calling_context ctx{
		.printVerbose = printVerbose,
		.info = info,
		.commandBuffer = commandBuffer,
		.commandBufferState = &state,
	};
	execute_rules(rules::selector_type::Draw, VK_NULL_HANDLE, ctx);

	if(!ctx.canceled) {
		dispatch.CmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
	}
}

void device::CmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	auto& state = commandBufferStates[commandBuffer];

	std::vector<VkImage> images;
	std::vector<VkBuffer> buffers;
	std::vector<rules::VkHandle> shaders;
	for(const auto& d : state.descriptorSets) {
		if(!d) {
			continue;
		}

		const auto& ds = descriptorStates[d];
		for(const auto& [_, binding] : ds.bindings) {
			switch(binding.type) {
				case rules::selector_type::Image:
					std::transform(binding.arrayElements.begin(), binding.arrayElements.end(), std::back_inserter(images), [](const auto& h){
						return (VkImage) h.handle;
					});
					break;
				default:
					;
			}
		}
	}

	auto& pstate = pipelineStates[state.pipeline];
	std::transform(pstate.stages.begin(), pstate.stages.end(), std::back_inserter(shaders), [](const auto& s){
		return s.customHandle;
	});
	buffers.push_back(state.indexBuffer);
	std::copy(state.vertexBuffers.begin(), state.vertexBuffers.end(), std::back_inserter(buffers));

	reflection::VkCmdDrawIndexed drawCall = {
		.indexCount = indexCount,
		.instanceCount = instanceCount,
		.firstIndex = firstIndex,
		.vertexOffset = vertexOffset,
		.firstInstance = firstInstance,
		.commandBufferState = {
			.scissorCount = static_cast<uint32_t>(state.scissors.size()),
			.pScissors = state.scissors.data(),
			.transformFeedback = state.transformFeedback
		}
	};
	rules::draw_info info = {images, shaders, buffers, state.indexBuffer, &drawCall};

	auto printVerbose = [&](spdlog::logger& logger){
		logger.info(
R"(CmdDrawIndexed: on device {} from command buffer {} with pipeline {}
  draw parameters:
	indexCount = {}
	instanceCount = {}
	firstIndex = {}
	vertexOffset = {}
	firstInstance = {}
{}
{}
{}
{}
{}
		)", fmt::ptr(state.device), fmt::ptr(commandBuffer), fmt::ptr(state.pipeline),
			indexCount, instanceCount, firstIndex, vertexOffset, firstInstance,
			verbose_commandbuffer_state(*this, state),
			verbose_pipeline_stages(pstate),
			verbose_vertex_bindings(*this, state, pstate, false),
			verbose_vertex_attributes(*this, pstate),
			verbose_descriptors(*this, state)
		);
	};

	rules::calling_context ctx{
		.printVerbose = printVerbose,
		.info = info,
		.commandBuffer = commandBuffer,
		.commandBufferState = &state,
	};
	execute_rules(rules::selector_type::Draw, VK_NULL_HANDLE, ctx);

	if(!ctx.canceled) {
		dispatch.CmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}
}

void device::CmdBeginTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets)
{
	auto& state = commandBufferStates[commandBuffer];
	state.transformFeedback = true;
	dispatch.CmdBeginTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

void device::CmdBindTransformFeedbackBuffersEXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes)
{
	auto& state = commandBufferStates[commandBuffer];
	if(state.transformFeedbackBuffers.size() < (firstBinding + bindingCount))
		state.transformFeedbackBuffers.resize(firstBinding + bindingCount);
	for(unsigned int i=0; i<bindingCount; i++) {
		state.transformFeedbackBuffers[i+firstBinding] = {pBuffers[i], pOffsets[i], pSizes[i]};
	}
	dispatch.CmdBindTransformFeedbackBuffersEXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
}

void device::CmdEndTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets)
{
	auto& state = commandBufferStates[commandBuffer];
	state.transformFeedback = false;
	state.transformFeedbackBuffers.clear();
	dispatch.CmdEndTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

VkResult device::EndCommandBuffer(VkCommandBuffer commandBuffer)
{
	auto& state = commandBufferStates[commandBuffer];
	state.transformFeedback = false;

	auto& map = inst->global_context.on_EndCommandBuffer;
	auto p = map.find(commandBuffer);
	if(p != map.end() && !p->second.empty()) {
		bool _1;
		std::vector<std::string> _2;
		std::string _3;
		std::vector<std::function<void(rules::VkHandle)>> _4;
		std::unordered_map<std::string, rules::data_value> _5;
		void* _6;

		rules::local_context ctx = {
			.logger = *logger,
			.instance = inst,
			.device = this,
			.commandBuffer = commandBuffer,
			.commandBufferState = &state,
			.canceled = _1,
			.overrides = _2,
			.customTag = _3,
			.creationCallbacks = _4,
			.local_variables = _5,
			.customPointer = _6
		};

		for(auto& f : p->second) {
			try {
				f(ctx);
			} catch(const std::exception& e) {
				logger->error("Failed to execute callback: {}", e.what());
			}
		}
		p->second.clear();
	}

	return dispatch.EndCommandBuffer(commandBuffer);
}

VkResult device::QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
	rules::present_info info = {pPresentInfo};
	rules::calling_context ctx{
		.info = info,
	};
	execute_rules(rules::selector_type::Present, VK_NULL_HANDLE, ctx);

	if(ctx.canceled)
		return VK_SUCCESS;
	return dispatch.QueuePresentKHR(queue, pPresentInfo);
}

VkResult device::QueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
{
	for(unsigned int i=0; i<submitCount; i++) {
		const VkSubmitInfo& submit = pSubmits[i];
		for(unsigned int j=0; j<submit.commandBufferCount; j++) {
			VkCommandBuffer commandBuffer = submit.pCommandBuffers[j];
			auto& state = commandBufferStates[commandBuffer];

			auto& map = inst->global_context.on_QueueSubmit;
			auto p = map.find(commandBuffer);
			if(p != map.end() && !p->second.empty()) {
				bool _1;
				std::vector<std::string> _2;
				std::string _3;
				std::vector<std::function<void(rules::VkHandle)>> _4;
				std::unordered_map<std::string, rules::data_value> _5;
				void* _6;

				rules::local_context ctx = {
					.logger = *logger,
					.instance = inst,
					.device = this,
					.commandBuffer = commandBuffer,
					.commandBufferState = &state,
					.canceled = _1,
					.overrides = _2,
					.customTag = _3,
					.creationCallbacks = _4,
					.local_variables = _5,
					.customPointer = _6
				};

				for(auto& f : p->second) {
					try {
						f(ctx);
					} catch(const std::exception& e) {
						logger->error("Failed to execute callback: {}", e.what());
					}
				}
				p->second.clear();
			}
		}
	}

	return dispatch.QueueSubmit(queue, submitCount, pSubmits, fence);
}

} // namespace CheekyLayer

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
	return CheekyLayer::get_device(commandBuffer).CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindPipeline(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipeline                                  pipeline)
{
	return CheekyLayer::get_device(commandBuffer).CmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindVertexBuffers(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets)
{
	return CheekyLayer::get_device(commandBuffer).CmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
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
	return CheekyLayer::get_device(commandBuffer).CmdBindVertexBuffers2EXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindIndexBuffer(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType)
{
	return CheekyLayer::get_device(commandBuffer).CmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdSetScissor(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstScissor,
    uint32_t                                    scissorCount,
    const VkRect2D*                             pScissors)
{
	return CheekyLayer::get_device(commandBuffer).CmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBeginRenderPass(
    VkCommandBuffer                             commandBuffer,
    const VkRenderPassBeginInfo*                pRenderPassBegin,
    VkSubpassContents                           contents)
{
	return CheekyLayer::get_device(commandBuffer).CmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdEndRenderPass(
    VkCommandBuffer                             commandBuffer)
{
	return CheekyLayer::get_device(commandBuffer).CmdEndRenderPass(commandBuffer);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdDrawIndexed(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    indexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstIndex,
    int32_t                                     vertexOffset,
    uint32_t                                    firstInstance)
{
	return CheekyLayer::get_device(commandBuffer).CmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdDraw(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    vertexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstVertex,
    uint32_t                                    firstInstance)
{
	return CheekyLayer::get_device(commandBuffer).CmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBeginTransformFeedbackEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstCounterBuffer,
    uint32_t                                    counterBufferCount,
    const VkBuffer*                             pCounterBuffers,
    const VkDeviceSize*                         pCounterBufferOffsets)
{
	return CheekyLayer::get_device(commandBuffer).CmdBeginTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdBindTransformFeedbackBuffersEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets,
    const VkDeviceSize*                         pSizes)
{
	return CheekyLayer::get_device(commandBuffer).CmdBindTransformFeedbackBuffersEXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
}

VK_LAYER_EXPORT void VKAPI_CALL CheekyLayer_CmdEndTransformFeedbackEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstCounterBuffer,
    uint32_t                                    counterBufferCount,
    const VkBuffer*                             pCounterBuffers,
    const VkDeviceSize*                         pCounterBufferOffsets)
{
	return CheekyLayer::get_device(commandBuffer).CmdEndTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_EndCommandBuffer(
    VkCommandBuffer                             commandBuffer)
{
	return CheekyLayer::get_device(commandBuffer).EndCommandBuffer(commandBuffer);
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
	return CheekyLayer::get_device(queue).QueuePresentKHR(queue, pPresentInfo);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_QueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    submitCount,
    const VkSubmitInfo*                         pSubmits,
    VkFence                                     fence)
{
	return CheekyLayer::get_device(queue).QueueSubmit(queue, submitCount, pSubmits, fence);
}
