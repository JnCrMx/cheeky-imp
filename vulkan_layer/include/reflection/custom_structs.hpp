#pragma once

#include "reflection/vkreflection.hpp"
#include <stdint.h>

namespace CheekyLayer { namespace reflection {
    struct VkCmdDrawIndexed {
        uint32_t indexCount;
        uint32_t instanceCount;
        uint32_t firstIndex;
        int32_t vertexOffset;
        uint32_t firstInstance;
    };

    #define VK_CMD_DRAW_INDEXED \
    { \
        "VkCmdDrawIndexed", \
		std::map<std::string, VkReflectInfo>{ \
			{"indexCount", VkReflectInfo{ .name = "indexCount", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCmdDrawIndexed, indexCount) }}, \
			{"instanceCount", VkReflectInfo{ .name = "instanceCount", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCmdDrawIndexed, instanceCount) }}, \
			{"firstIndex", VkReflectInfo{ .name = "firstIndex", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCmdDrawIndexed, firstIndex) }}, \
			{"vertexOffset", VkReflectInfo{ .name = "vertexOffset", .type = "int32_t", .pointer = false, .offset = offsetof(VkCmdDrawIndexed, vertexOffset) }}, \
			{"firstInstance", VkReflectInfo{ .name = "firstInstance", .type = "uint32_t", .pointer = false, .offset = offsetof(VkCmdDrawIndexed, firstInstance) }}, \
        } \
    }

    #define VK_CUSTOM_STRUCTS \
        VK_CMD_DRAW_INDEXED
}}