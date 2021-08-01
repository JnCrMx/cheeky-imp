#include "rules/actions.hpp"
#include "dispatch.hpp"
#include "logger.hpp"
#include "layer.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "utils.hpp"
#include "images.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <istream>
#include <iterator>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>

#ifdef USE_IMAGE_TOOLS
#include <block_compression.hpp>
#include <image.hpp>

#include <stb_image.h>
#endif

namespace CheekyLayer
{
	action_register<mark_action> mark_action::reg("mark");
	action_register<unmark_action> unmark_action::reg("unmark");
	action_register<verbose_action> verbose_action::reg("verbose");
	action_register<sequence_action> sequence_action::reg("seq");
	action_register<each_action> each_action::reg("each");
	action_register<on_action> on_action::reg("on");
	action_register<disable_action> disable_action::reg("disable");
	action_register<cancel_action> cancel_action::reg("cancel");
	action_register<log_action> log_action::reg("log");
	action_register<log_extended_action> log_extended_action::reg("logx");
	action_register<override_action> override_action::reg("override");
	action_register<socket_action> socket_action::reg("socket");
	action_register<write_action> write_action::reg("write");
	action_register<store_action> store_action::reg("store");
	action_register<overload_action> overload_action::reg("overload");

	void mark_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule&)
	{
		rule_env.marks[(VkHandle)handle].push_back(m_mark);
		ctx.logger << " Marked " << to_string(type) << " " << handle << " as \"" << m_mark << "\" ";
	}

	void mark_action::read(std::istream& in)
	{
		std::getline(in, m_mark, ')');
	}

	std::ostream& mark_action::print(std::ostream& out)
	{
		out << "mark(" << m_mark << ")";
		return out;
	}

	void unmark_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule&)
	{
		if(m_clear)
		{
			rule_env.marks[(VkHandle)handle].clear();
			ctx.logger << " Cleared marks of " << to_string(type) << " " << handle;
		}
		else
		{
			auto& marks = rule_env.marks[(VkHandle)handle];
			auto p = std::find(marks.begin(), marks.end(), m_mark);
			if(p != marks.end())
			{
				marks.erase(p);
				ctx.logger << " Unmarked " << to_string(type) << " " << handle << " as \"" << m_mark << "\" ";
			}
		}
	}

	void unmark_action::read(std::istream& in)
	{
		std::getline(in, m_mark, ')');
		if(m_mark == "*")
			m_clear = true;
	}

	std::ostream& unmark_action::print(std::ostream& out)
	{
		out << "unmark(" << (m_clear ? "*" : m_mark) << ")";
		return out;
	}

	void verbose_action::execute(selector_type, VkHandle, local_context& ctx, rule&)
	{
		if(ctx.printVerbose.has_value())
			ctx.printVerbose.value()(ctx.logger);
	}

	void verbose_action::read(std::istream& in)
	{
		check_stream(in, ')');
	}

	std::ostream& verbose_action::print(std::ostream& out)
	{
		out << "verbose()";
		return out;
	}

	void sequence_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		for(auto& a : m_actions)
		{
			a->execute(type, handle, ctx, rule);
		}
	}

	void sequence_action::read(std::istream& in)
	{
		while(in.peek() != ')')
		{
			m_actions.push_back(read_action(in, m_type));

			skip_ws(in);
			char seperator = in.peek();
			if(seperator != ',')
				continue;
			in.get();
			skip_ws(in);
		}

		check_stream(in, ')');
	}

	std::ostream& sequence_action::print(std::ostream &out)
	{
		out << "seq(";
		for(int i=0; i<m_actions.size(); i++)
		{
			m_actions[i]->print(out);
			if(i != m_actions.size()-1)
				out << ", ";
		}
		out << ")";
		return out;
	}

	void each_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		std::vector<VkHandle> handles;
		if(type == selector_type::Draw)
		{
			switch(m_selector->get_type())
			{
				case selector_type::Image:
					std::copy(ctx.info->draw.images.begin(), ctx.info->draw.images.end(), std::back_inserter(handles));
					break;
				case selector_type::Shader:
					std::copy(ctx.info->draw.shaders.begin(), ctx.info->draw.shaders.end(), std::back_inserter(handles));
					break;
				case selector_type::Buffer:
					std::copy(ctx.info->draw.vertexBuffers.begin(), ctx.info->draw.vertexBuffers.end(), std::back_inserter(handles));
					handles.push_back(ctx.info->draw.indexBuffer);
					break;
				default:
					throw std::runtime_error("unsupported selector type: "+std::to_string(m_selector->get_type()));
			}
		}
		for(auto h : handles)
		{
			if(m_selector->test(m_selector->get_type(), h, ctx))
				m_action->execute(m_selector->get_type(), h, ctx, rule);
		}
	}

	void each_action::read(std::istream& in)
	{
		m_selector = std::make_unique<selector>();
		in >> *m_selector;

		selector_type type = m_selector->get_type();
		if(type != selector_type::Image && type != selector_type::Buffer && type != selector_type::Shader)
			throw std::runtime_error("unsupported selector type "+to_string(type)+" only image, buffer and shader selectors are supported");

		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		m_action = read_action(in, type);
		check_stream(in, ')');
	}

	std::ostream& each_action::print(std::ostream& out)
	{
		out << "each(";
		m_selector->print(out);
		out << ", ";
		m_action->print(out);
		out << ")";
		return out;
	}

	void on_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(!ctx.commandBuffer)
		{
			ctx.logger << " Cannot schedule action, because CommandBuffer is not known. We will execute it right now instead.";
			m_action->execute(type, handle, ctx, rule);
			return;
		}

		switch(m_event)
		{
			case EndCommandBuffer:
				rule_env.onEndCommandBuffer(ctx.commandBuffer, [this, type, handle, &rule](local_context& ctx2){
					this->m_action->execute(type, handle, ctx2, rule);
				});
				break;
			case QueueSubmit:
				rule_env.onQueueSubmit(ctx.commandBuffer, [this, type, handle, &rule](local_context& ctx2){
					this->m_action->execute(type, handle, ctx2, rule);
				});
				break;
		}
	}

	void on_action::read(std::istream& in)
	{
		std::string event;
		std::getline(in, event, ',');
		m_event = on_action_event_from_string(event);

		skip_ws(in);

		m_action = read_action(in, m_type);
		check_stream(in, ')');
	}

	std::ostream& on_action::print(std::ostream& out)
	{
		out << "on(" << on_action_event_to_string(m_event) << ", ";
		m_action->print(out);
		out << ")";
		return out;
	}

	void disable_action::execute(selector_type, VkHandle, local_context&, rule& rule)
	{
		rule.disable();
	}

	void disable_action::read(std::istream& in)
	{
		check_stream(in, ')');
	}

	std::ostream& disable_action::print(std::ostream& out)
	{
		out << "disable()";
		return out;
	}

	void cancel_action::execute(selector_type, VkHandle, local_context& ctx, rule&)
	{
		ctx.canceled = true;
	}

	void cancel_action::read(std::istream& in)
	{
		check_stream(in, ')');
	}

	std::ostream& cancel_action::print(std::ostream& out)
	{
		out << "cancel()";
		return out;
	}

	void log_action::execute(selector_type stype, VkHandle handle, local_context& ctx, rule&)
	{
		std::string s = m_text;
		replace(s, "$]", ")");
		replace(s, "$[", "(");

		replace(s, "$type", to_string(stype));

		{
			std::stringstream oss;
			oss << handle;
			replace(s, "$handle", oss.str());
		}

		{
			auto p = rule_env.marks.find(handle);
			if(p != rule_env.marks.find(handle))
			{
				auto& v = p->second;

				std::stringstream oss;
				oss << "[";
				for(int i=0; i<v.size(); i++)
				{
					replace(s, "$marks["+std::to_string(i)+"]", v[i]);
					oss << v[i] << (i == v.size()-1 ? "]" : ",");
				}
				replace(s, "$marks[*]", oss.str());
			}
		}

		ctx.logger << s;
	}

	void log_action::read(std::istream& in)
	{
		std::getline(in, m_text, ')');
	}

	std::ostream& log_action::print(std::ostream& out)
	{
		out << "log(" << m_text << ")";
		return out;
	}

	void log_extended_action::execute(selector_type stype, VkHandle handle, local_context& ctx, rule& rule)
	{
		ctx.logger << std::get<std::string>(m_data->get(stype, data_type::String, handle, ctx, rule));
	}

	void log_extended_action::read(std::istream& in)
	{
		m_data = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_data->supports(m_type, data_type::String))
			throw std::runtime_error("data does not support strings");
	}

	std::ostream& log_extended_action::print(std::ostream& out)
	{
		out << "logx(";
		m_data->print(out);
		out << ")";
		return out;
	}

	void override_action::execute(selector_type, VkHandle, local_context& ctx, rule &)
	{
		ctx.overrides.push_back(m_expression);
	}

	void override_action::read(std::istream& in)
	{
		std::getline(in, m_expression, ')');
	}

	std::ostream& override_action::print(std::ostream& out)
	{
		out << "override(" << m_expression << ")";
		return out;
	}

	void socket_action::execute(selector_type, VkHandle, local_context &, rule &)
	{
		if(rule_env.fds.count(m_name))
			throw std::runtime_error("file descriptor with name "+m_name+" already exists");

		rule_env.fds[m_name] = std::make_unique<ipc::socket>(m_socketType, m_host, m_port, m_protocol);
		rule_env.fds[m_name]->m_name = m_name;
	}

	void socket_action::read(std::istream& in)
	{
		std::getline(in, m_name, ',');
		skip_ws(in);

		std::string type;
		std::getline(in, type, ',');
		m_socketType = ipc::socket::socket_type_from_string(type);
		skip_ws(in);

		std::getline(in, m_host, ',');
		skip_ws(in);

		in >> m_port;
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		std::string protocol;
		std::getline(in, protocol, ')');
		m_protocol = ipc::socket::protocol_type_from_string(protocol);
	}

	std::ostream& socket_action::print(std::ostream& out)
	{
		out << "socket(" << m_name << ", " << ipc::socket::socket_type_to_string(m_socketType) << ", " << m_host << ", " << m_port
			<< ", " << ipc::socket::protocol_type_to_string(m_protocol) << ")";
		return out;
	}

	void write_action::execute(selector_type stype, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(!rule_env.fds.count(m_fd))
			throw std::runtime_error("file descriptor with name "+m_fd+" does not exists");

		auto& fd = rule_env.fds[m_fd];

		if(m_data->supports(stype, data_type::Raw))
		{
			data_value val = m_data->get(stype, data_type::Raw, handle, ctx, rule);
			auto& v = std::get<std::vector<uint8_t>>(val);
			if(fd->write(v) < 0)
				throw std::runtime_error("failed to send data to file descriptor \""+m_fd+"\": " + strerror(errno));
		}
		else if(m_data->supports(stype, data_type::String))
		{
			data_value val = m_data->get(stype, data_type::String, handle, ctx, rule);
			auto& s = std::get<std::string>(val);
			std::vector<uint8_t> v(s.begin(), s.end());
			if(fd->write(v) < 0)
				throw std::runtime_error("failed to write data to file descriptor \""+m_fd+"\": " + strerror(errno));
		}
		else
			throw std::runtime_error("no suitable data type available");
	}

	void write_action::read(std::istream& in)
	{
		std::getline(in, m_fd, ',');
		skip_ws(in);

		m_data = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_data->supports(m_type, data_type::String) && !m_data->supports(m_type, data_type::Raw))
			throw std::runtime_error("data does not support strings or raw data");
	}

	std::ostream& write_action::print(std::ostream& out)
	{
		out << "write(" << m_fd << ", ";
		m_data->print(out);
		out << ")";
		return out;
	}

	void store_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule &)
	{
		rule_env.handles[m_name] = { handle , type, ctx.device };
	}

	void store_action::read(std::istream& in)
	{
		std::getline(in, m_name, ')');
	}

	std::ostream& store_action::print(std::ostream& out)
	{
		out << "store(" << m_name << ")";
		return out;
	}

	void overload_action::execute(selector_type stype, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(!rule_env.handles.count(m_target))
			throw std::runtime_error("stored handle with name "+m_target+" does not exists");

		scoped_lock l(global_lock);
		if(m_mode == mode::Data)
		{
			std::vector<uint8_t> data = std::get<std::vector<uint8_t>>(m_data->get(stype, data_type::Raw, handle, ctx, rule));
			std::thread t(&overload_action::workTry, this, std::string{}, data);
			rule_env.threads.push_back(std::move(t));
		}
		else if(m_mode == mode::FileFromData)
		{
			std::string filename = std::get<std::string>(m_data->get(stype, data_type::String, handle, ctx, rule));
			std::thread t(&overload_action::workTry, this, filename, std::vector<uint8_t>{});
			rule_env.threads.push_back(std::move(t));
		}
		else
		{
			std::thread t(&overload_action::workTry, this, m_filename, std::vector<uint8_t>{});
			rule_env.threads.push_back(std::move(t));
		}
	}

	void overload_action::workTry(std::string optFilename, std::vector<uint8_t> optData)
	{
		try
		{
			work(optFilename, std::move(optData));
		}
		catch(const std::exception& ex)
		{
			*::logger << logger::begin << logger::error << " overload failed: " << ex.what() << logger::end;
		}
	}
	void overload_action::work(std::string optFilename, std::vector<uint8_t> optData)
	{
		stored_handle& h = rule_env.handles[m_target];

		VkResult result;
		std::vector<uint8_t> data(16);
		std::vector<VkDeviceSize> offsets;

		if(m_mode == mode::Data)
		{
			data = optData;
		}
		else
		{
#ifdef USE_IMAGE_TOOLS
			bool imageTools = true;
#else
			bool imageTools = false;
#endif
			if(h.type == selector_type::Image && optFilename.ends_with(".png") && imageTools)
			{
				VkImageCreateInfo imgInfo = images[(VkImage)h.handle];

				int w, h, comp;
				uint8_t* buf = stbi_load(optFilename.c_str(), &w, &h, &comp, STBI_rgb_alpha);
				image_tools::image image(w, h, buf);

				if(imgInfo.mipLevels <= 1)
				{
					offsets.push_back(0);
					image_tools::compress(imgInfo.format, image, data, imgInfo.extent.width, imgInfo.extent.height);
				}
				else
				{
					data.clear();
					for(int i=0; i<imgInfo.mipLevels; i++)
					{
						offsets.push_back(data.size());

						std::vector<uint8_t> v;
						image_tools::compress(imgInfo.format, image, v, imgInfo.extent.width / (1<<i), imgInfo.extent.height / (1<<i));
						std::copy(v.begin(), v.end(), std::back_inserter(data));
					}
				}

				free(buf);
			}
			else if(h.type == selector_type::Image && optFilename.find("${mip}") != std::string::npos)
			{
				VkImageCreateInfo imgInfo = images[(VkImage)h.handle];
				data.clear();
				for(int i=0; i<imgInfo.mipLevels; i++)
				{
					auto offset = data.size();
					offsets.push_back(offset);

					std::string filename = optFilename;
					replace(filename, "${mip}", std::to_string(i));
					std::ifstream in(filename, std::ios_base::binary | std::ios_base::ate);

					auto size = in.tellg();
					data.resize(data.size()+size);
					in.seekg(0);
					in.read((char*)(data.data() + offset), size);
				}
			}
			else
			{
				offsets.push_back(0);

				std::ifstream in(optFilename, std::ios_base::binary | std::ios_base::ate);

				auto size = in.tellg();
				data.resize(size);
				in.seekg(0);
				in.read((char*)data.data(), size);
			}
		}

		VkBufferCreateInfo bufferInfo;
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = nullptr;
		bufferInfo.flags = 0;
		bufferInfo.size = data.size();
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.queueFamilyIndexCount = 0;
		bufferInfo.pQueueFamilyIndices = nullptr;
		VkBuffer buffer;
		if((result = device_dispatch[GetKey(h.device)].CreateBuffer(h.device, &bufferInfo, nullptr, &buffer)) != VK_SUCCESS)
			throw std::runtime_error("failed to create staging buffer: "+vk::to_string((vk::Result)result));

		VkMemoryRequirements requirements;
		device_dispatch[GetKey(h.device)].GetBufferMemoryRequirements(h.device, buffer, &requirements);

		VkMemoryAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		allocateInfo.memoryTypeIndex = findMemoryType(deviceInfos[h.device].memory, requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		allocateInfo.allocationSize = requirements.size;
		VkDeviceMemory memory;
		if(device_dispatch[GetKey(h.device)].AllocateMemory(h.device, &allocateInfo, nullptr, &memory) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate memory for staging buffer");

		if(device_dispatch[GetKey(h.device)].BindBufferMemory(h.device, buffer, memory, 0) != VK_SUCCESS)
			throw std::runtime_error("failed to bind memory to staging buffer");

		void* bufferPointer;
		if(device_dispatch[GetKey(h.device)].MapMemory(h.device, memory, 0, data.size(), 0, &bufferPointer) != VK_SUCCESS)
			throw std::runtime_error("failed to map staging memory");
		memcpy(bufferPointer, data.data(), data.size());
		device_dispatch[GetKey(h.device)].UnmapMemory(h.device, memory);

		{
			scoped_lock l(transfer_lock);

			VkCommandBuffer commandBuffer = transferCommandBuffers[h.device];
			if(commandBuffer == VK_NULL_HANDLE)
				throw std::runtime_error("command buffer is NULL");

			if(device_dispatch[GetKey(h.device)].ResetCommandBuffer(commandBuffer, 0) != VK_SUCCESS)
				throw std::runtime_error("failed to reset command buffer");

			VkCommandBufferBeginInfo beginInfo;
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.pNext = nullptr;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			beginInfo.pInheritanceInfo = nullptr;
			if(device_dispatch[GetKey(h.device)].BeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
				throw std::runtime_error("failed to begin the command buffer");

			if(h.type == selector_type::Buffer)
			{
				VkBufferCopy copyRegion{};
				copyRegion.srcOffset = 0; // Optional
				copyRegion.dstOffset = 0; // Optional
				copyRegion.size = data.size();
				device_dispatch[GetKey(h.device)].CmdCopyBuffer(commandBuffer, buffer, (VkBuffer)h.handle, 1, &copyRegion);
			}
			else if(h.type == selector_type::Image)
			{
				VkImageCreateInfo imgInfo = images[(VkImage)h.handle];

				VkImageMemoryBarrier memoryBarrier{};
				memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				memoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				memoryBarrier.image = (VkImage) h.handle;
				memoryBarrier.srcAccessMask = 0;
				memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				memoryBarrier.subresourceRange.layerCount = imgInfo.arrayLayers;
				memoryBarrier.subresourceRange.levelCount = imgInfo.mipLevels;
				memoryBarrier.subresourceRange.baseArrayLayer = 0;
				memoryBarrier.subresourceRange.baseMipLevel = 0;
				memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				memoryBarrier.pNext = nullptr;

				device_dispatch[GetKey(h.device)].CmdPipelineBarrier(commandBuffer,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);

				std::vector<VkBufferImageCopy> copies(offsets.size());
				for(int i=0; i<copies.size(); i++)
				{
					VkBufferImageCopy copy{};
					copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					copy.imageSubresource.baseArrayLayer = 0;
					copy.imageSubresource.layerCount = 1;
					copy.imageSubresource.mipLevel = i;
					copy.imageExtent = { .width = imgInfo.extent.width / (1<<i), .height = imgInfo.extent.height / (1<<i), .depth = 1 };
					copy.bufferOffset = offsets[i];

					copies[i] = copy;
				}
				device_dispatch[GetKey(h.device)].CmdCopyBufferToImage(commandBuffer, buffer, (VkImage)h.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					copies.size(), copies.data());
			}

			if(device_dispatch[GetKey(h.device)].EndCommandBuffer(commandBuffer) != VK_SUCCESS)
				throw std::runtime_error("failed to end command buffer");

			VkQueue queue = transferQueues[h.device];
			if(queue == VK_NULL_HANDLE)
				throw std::runtime_error("command buffer is NULL");

			if(device_dispatch[GetKey(h.device)].QueueWaitIdle(queue) != VK_SUCCESS)
				throw std::runtime_error("cannot wait for queue to be idle before copying");

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;
			if(device_dispatch[GetKey(h.device)].QueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
				throw std::runtime_error("failed to submit command buffer");

			if(device_dispatch[GetKey(h.device)].QueueWaitIdle(queue) != VK_SUCCESS)
				throw std::runtime_error("cannot wait for queue to be idle after copying");

			device_dispatch[GetKey(h.device)].DestroyBuffer(h.device, buffer, nullptr);
			device_dispatch[GetKey(h.device)].FreeMemory(h.device, memory, nullptr);
		}
	}

	void overload_action::read(std::istream& in)
	{
		std::getline(in, m_target, ',');
		skip_ws(in);

		std::string mode;
		std::getline(in, mode, ',');
		skip_ws(in);
		if(mode == "File")
		{
			m_mode = mode::File;

			std::getline(in, m_filename, ')');
		}
		else if(mode == "FileFromData")
		{
			m_mode = mode::FileFromData;

			m_data = read_data(in, m_type);
			check_stream(in, ')');

			if(!m_data->supports(m_type, data_type::String))
				throw std::runtime_error("data does not support string data");
		}
		else if(mode == "Data")
		{
			m_mode = mode::Data;

			m_data = read_data(in, m_type);
			check_stream(in, ')');

			if(!m_data->supports(m_type, data_type::Raw))
				throw std::runtime_error("data does not support raw data");
		}
		else
			throw std::runtime_error("mode "+mode+" is not supported, must be either File, FileFromData or Data");
	}

	std::ostream& overload_action::print(std::ostream& out)
	{
		out << "overload(" << m_target << ", ";
		switch(m_mode)
		{
			case File:
				out << "File, " << m_filename;
				break;
			case FileFromData:
				out << "FileFromData, ";
				m_data->print(out);
				break;
			case Data:
				out << "Data, ";
				m_data->print(out);
				break;
		}
		out << ")";
		return out;
	}
}
