#pragma once

#include "reflection/vkreflection.hpp"
#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace CheekyLayer { namespace reflection {
	struct VkCommandBufferState {
		uint32_t scissorCount;
		VkRect2D *pScissors;
	};

	struct VkCmdDrawIndexed {
		uint32_t indexCount;
		uint32_t instanceCount;
		uint32_t firstIndex;
		int32_t vertexOffset;
		uint32_t firstInstance;

		VkCommandBufferState commandBufferState;
	};

	struct VkCmdDraw {
		uint32_t vertexCount;
		uint32_t instanceCount;
		uint32_t firstVertex;
		uint32_t firstInstance;

		VkCommandBufferState commandBufferState;
	};

	#define VK_COMMAND_BUFFER_STATE \
	{ \
		"VkCommandBufferState", \
		VkTypeInfo{ \
			.name = "VkCommandBufferState", \
			.size = sizeof(VkCommandBufferState), \
			.members = inner_reflection_map{ \
				{"scissorCount", VkReflectInfo{ .name = "scissorCount", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCommandBufferState, scissorCount) }}, \
				{"pScissors", VkReflectInfo{ .name = "pScissors", .type = "VkRect2D", .pointer = true, .array = true, .arrayLength = "scissorCount", .offset = offsetof(VkCommandBufferState, pScissors) }}, \
			} \
		} \
	}

	#define VK_CMD_DRAW_INDEXED \
	{ \
		"VkCmdDrawIndexed", \
		VkTypeInfo{ \
			.name = "VkCmdDrawIndexed", \
			.size = sizeof(VkCmdDrawIndexed), \
			.members = inner_reflection_map{ \
				{"indexCount", VkReflectInfo{ .name = "indexCount", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCmdDrawIndexed, indexCount) }}, \
				{"instanceCount", VkReflectInfo{ .name = "instanceCount", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCmdDrawIndexed, instanceCount) }}, \
				{"firstIndex", VkReflectInfo{ .name = "firstIndex", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCmdDrawIndexed, firstIndex) }}, \
				{"vertexOffset", VkReflectInfo{ .name = "vertexOffset", .type = "int32_t", .pointer = false, .offset = offsetof(VkCmdDrawIndexed, vertexOffset) }}, \
				{"firstInstance", VkReflectInfo{ .name = "firstInstance", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCmdDrawIndexed, firstInstance) }}, \
				{"commandBufferState", VkReflectInfo{ .name = "commandBufferState", .type = "VkCommandBufferState", .pointer = false, .offset = offsetof(VkCmdDrawIndexed, commandBufferState) }}, \
			} \
		} \
	}

	#define VK_CMD_DRAW \
	{ \
		"VkCmdDraw", \
		VkTypeInfo{ \
			.name = "VkCmdDraw", \
			.size = sizeof(VkCmdDraw), \
			.members = inner_reflection_map{ \
				{"vertexCount", VkReflectInfo{ .name = "vertexCount", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCmdDraw, vertexCount) }}, \
				{"instanceCount", VkReflectInfo{ .name = "instanceCount", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCmdDraw, instanceCount) }}, \
				{"firstVertex", VkReflectInfo{ .name = "firstVertex", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCmdDraw, firstVertex) }}, \
				{"firstInstance", VkReflectInfo{ .name = "firstInstance", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCmdDraw, firstInstance) }}, \
				{"commandBufferState", VkReflectInfo{ .name = "commandBufferState", .type = "VkCommandBufferState", .pointer = false, .offset = offsetof(VkCmdDraw, commandBufferState) }}, \
			} \
		} \
	}

	#define VK_CUSTOM_STRUCTS \
		VK_COMMAND_BUFFER_STATE, \
		VK_CMD_DRAW_INDEXED, \
		VK_CMD_DRAW
}}