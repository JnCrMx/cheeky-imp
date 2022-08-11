#include "rules/actions.hpp"
#include "dispatch.hpp"
#include "logger.hpp"
#include "layer.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "utils.hpp"
#include "images.hpp"
#include "draw.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <glm/common.hpp>
#include <ios>
#include <istream>
#include <iterator>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <experimental/iterator>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <glm/packing.hpp>
#include <glm/gtc/packing.hpp>

#ifdef USE_IMAGE_TOOLS
#include <block_compression.hpp>
#include <image.hpp>

#include <stb_image.h>
#include <stb_image_write.h>
#endif

namespace CheekyLayer::rules::actions
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
	action_register<preload_action> preload_action::reg("preload");
	action_register<dump_framebuffer_action> dump_framebuffer_action::reg("dumbfb");
	action_register<every_action> every_action::reg("every");
	action_register<buffer_copy_action> buffer_copy_action::reg("buffer_copy");
	action_register<set_global_action> set_global_action::reg("set_global");
	action_register<set_global_action> set_global_action::reg2("global=");
	action_register<set_local_action> set_local_action::reg("set_local");
	action_register<set_local_action> set_local_action::reg2("local=");

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

		const auto locals = ctx.local_variables;
		ctx.logger << " Saving locals: ";
		std::transform(locals.begin(), locals.end(), std::experimental::make_ostream_joiner(ctx.logger.raw(), ", "), [](const auto& a){
			return a.first;
		});

		switch(m_event)
		{
			case EndCommandBuffer:
				rule_env.onEndCommandBuffer(ctx.commandBuffer, [this, type, handle, &rule, locals](local_context& ctx2){
					local_context deref = ctx2;
					deref.local_variables = locals;
					this->m_action->execute(type, handle, deref, rule);
				});
				break;
			case QueueSubmit:
				rule_env.onQueueSubmit(ctx.commandBuffer, [this, type, handle, &rule, locals](local_context& ctx2){
					local_context deref = ctx2;
					deref.local_variables = locals;
					this->m_action->execute(type, handle, deref, rule);
				});
				break;
			case EndRenderPass:
				rule_env.onEndRenderPass(ctx.commandBuffer, [this, type, handle, &rule, locals](local_context& ctx2){
					local_context deref = ctx2;
					deref.local_variables = locals;
					this->m_action->execute(type, handle, deref, rule);
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

#ifdef USE_IMAGE_TOOLS
	bool imageTools = true;
#else
	bool imageTools = false;
#endif
	// cache for preloading (must be kinda specific, because format, extent and mipLevels must match)
	struct image_key
	{
		std::string filename;
		VkFormat format;
		uint32_t width;
		uint32_t height;
		uint32_t mipLevels;
    	bool operator<(const image_key& other) const
		{
			return std::tie(filename, format, width, height, mipLevels) < std::tie(other.filename, other.format, other.width, other.height, other.mipLevels);
		}
	};
	std::map<image_key, std::pair<std::vector<uint8_t>, std::vector<VkDeviceSize>>> imageFileCache;
	void overload_action::work(std::string optFilename, std::vector<uint8_t> optData)
	{
		stored_handle& h = rule_env.handles[m_target];

		VkResult result;
		std::vector<uint8_t> data(16);
		std::vector<VkDeviceSize> offsets;

		auto t0 = std::chrono::high_resolution_clock::now();
		if(m_mode == mode::Data)
		{
			data = optData;
		}
		else
		{
			if(h.type == selector_type::Image && optFilename.ends_with(".png") && imageTools)
			{
				VkImageCreateInfo imgInfo = images[(VkImage)h.handle];
				decltype(imageFileCache.begin()->first) cacheKey = {optFilename, imgInfo.format, imgInfo.extent.width, imgInfo.extent.height, imgInfo.mipLevels};
				if(imageFileCache.contains(cacheKey))
				{
					data = imageFileCache[cacheKey].first;
					offsets = imageFileCache[cacheKey].second;
				}
				else
				{
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

					stbi_image_free(buf);
					imageFileCache[cacheKey] = {data, offsets}; // do we want to store here? it needs so much RAM T_T
				}
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
		auto tDataReady = std::chrono::high_resolution_clock::now();

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

		auto tStagingReady = std::chrono::high_resolution_clock::now();

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

			auto tCommandReady = std::chrono::high_resolution_clock::now();

			VkQueue queue = transferQueues[h.device];
			if(queue == VK_NULL_HANDLE)
				throw std::runtime_error("command buffer is NULL");

			if(device_dispatch[GetKey(h.device)].QueueWaitIdle(queue) != VK_SUCCESS)
				throw std::runtime_error("cannot wait for queue to be idle before copying");
			auto tIdle1 = std::chrono::high_resolution_clock::now();

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;
			if(device_dispatch[GetKey(h.device)].QueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
				throw std::runtime_error("failed to submit command buffer");

			if(device_dispatch[GetKey(h.device)].QueueWaitIdle(queue) != VK_SUCCESS)
				throw std::runtime_error("cannot wait for queue to be idle after copying");
			auto tIdle2 = std::chrono::high_resolution_clock::now();

			device_dispatch[GetKey(h.device)].DestroyBuffer(h.device, buffer, nullptr);
			device_dispatch[GetKey(h.device)].FreeMemory(h.device, memory, nullptr);

			*::logger << logger::begin << "overload timing:\n"
				<< "\t" << "image load: " << std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(tDataReady - t0).count()) << "ms\n"
				<< "\t" << "staging buffer: " << std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(tStagingReady - tDataReady).count()) << "ms\n"
				<< "\t" << "command buffer: " << std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(tCommandReady - tStagingReady).count()) << "ms\n"
				<< "\t" << "queue wait: " << std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(tIdle1 - tCommandReady).count()) << "ms\n"
				<< "\t" << "copy operation: " << std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(tIdle2 - tIdle1).count()) << "ms"
				<< logger::end;
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

	void preload_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		std::string filename = std::get<std::string>(m_filename->get(type, data_type::String, handle, ctx, rule));
		std::thread t(&preload_action::work, this, filename);
		rule_env.threads.push_back(std::move(t));
	}

	void preload_action::work(std::string optFilename)
	{
		stored_handle& h = rule_env.handles[m_target];
		try
		{
			if(h.type == selector_type::Image && optFilename.ends_with(".png") && imageTools)
			{
				VkImageCreateInfo imgInfo = images[(VkImage)h.handle];
				decltype(imageFileCache.begin()->first) cacheKey = {optFilename, imgInfo.format, imgInfo.extent.width, imgInfo.extent.height, imgInfo.mipLevels};
				if(imageFileCache.contains(cacheKey))
				{
					return;
				}
				else
				{
					std::vector<uint8_t> data;
					std::vector<VkDeviceSize> offsets;

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

					stbi_image_free(buf);
					imageFileCache[cacheKey] = {data, offsets};
				}
			}
		}
		catch(const std::exception& ex)
		{
			*::logger << logger::begin << logger::error << " preload failed: " << ex.what() << logger::end;
		}
	}

	void preload_action::read(std::istream& in)
	{
		std::getline(in, m_target, ',');
		skip_ws(in);

		m_filename = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_filename->supports(m_type, data_type::String))
			throw std::runtime_error("data does not support string data");
	}

	std::ostream& preload_action::print(std::ostream& out)
	{
		out << "preload(" << m_target << ", ";
		m_filename->print(out);
		out << ")";
		return out;
	}

	void dump_framebuffer_action::execute(selector_type, VkHandle, local_context &ctx, rule&)
	{
		VkDevice device = ctx.device;

		VkBuffer buffer;
		VkDeviceMemory memory;
		VkEvent event;

		VkFramebuffer fb = ctx.commandBufferState->framebuffer;
		VkImageView view = framebuffers[fb].attachments[m_attachment];
		VkImage image = imageViews[view];
		VkImageCreateInfo imageInfo = images[image];

		VkMemoryRequirements req;
		device_dispatch[GetKey(device)].GetImageMemoryRequirements(ctx.device, image, &req);
		VkDeviceSize size = req.size;

		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = size;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		device_dispatch[GetKey(device)].CreateBuffer(ctx.device, &bufferCreateInfo, nullptr, &buffer);

		device_dispatch[GetKey(device)].GetBufferMemoryRequirements(ctx.device, buffer, &req);

		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = req.size;
		allocateInfo.memoryTypeIndex = findMemoryType(deviceInfos[ctx.device].memory, 
			req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		device_dispatch[GetKey(device)].AllocateMemory(ctx.device, &allocateInfo, nullptr, &memory);

		device_dispatch[GetKey(device)].BindBufferMemory(ctx.device, buffer, memory, 0);

		VkEventCreateInfo eventCreateInfo{};
		eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
		device_dispatch[GetKey(device)].CreateEvent(device, &eventCreateInfo, nullptr, &event);

		VkBufferImageCopy copy{};
		copy.imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copy.imageExtent = imageInfo.extent;

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.subresourceRange = VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

		device_dispatch[GetKey(device)].CmdPipelineBarrier(ctx.commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr, 0, nullptr, 1, &barrier);
		device_dispatch[GetKey(device)].CmdCopyImageToBuffer(ctx.commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
		device_dispatch[GetKey(device)].CmdSetEvent(ctx.commandBuffer, event, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

		std::thread thread([this, device, imageInfo, size, buffer, memory, event](){
			for(;;)
			{
				if(device_dispatch[GetKey(device)].GetEventStatus(device, event) == VK_EVENT_SET)
					break;
				using std::chrono::operator""ms;
				std::this_thread::sleep_for(10ms);
			}

			{
				std::vector<char> data(size);

				{
					scoped_lock l(global_lock);
					char* p;
					device_dispatch[GetKey(device)].MapMemory(device, memory, 0, size, 0, (void**)&p);
					std::copy(p, p+size, data.begin());
					device_dispatch[GetKey(device)].UnmapMemory(device, memory);

					device_dispatch[GetKey(device)].DestroyBuffer(device, buffer, nullptr);
					device_dispatch[GetKey(device)].FreeMemory(device, memory, nullptr);
					device_dispatch[GetKey(device)].DestroyEvent(device, event, nullptr);
				}

				auto t = std::chrono::high_resolution_clock::now();
				std::chrono::duration<long> seconds = std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch());

				std::string name = std::to_string(seconds.count()) + "_" + std::to_string(m_attachment);

#ifdef USE_IMAGE_TOOLS
				bool decompression = image_tools::is_decompression_supported(imageInfo.format);
				if(decompression || 
					imageInfo.format == VK_FORMAT_R8G8B8A8_SRGB ||
					imageInfo.format == VK_FORMAT_R16G16B16A16_SFLOAT ||
					imageInfo.format == VK_FORMAT_R16G16_SFLOAT ||
					imageInfo.format == VK_FORMAT_R16G16B16A16_UINT)
				{
					std::string filename = global_config["dumpDirectory"]+"/images/framebuffers/"+name+".png";
					try
					{
						int w = imageInfo.extent.width;
						int h = imageInfo.extent.height;

						image_tools::image image(w, h);

						if(decompression)
							image_tools::decompress(imageInfo.format, (uint8_t*)data.data(), image, w, h);
						else if(imageInfo.format == VK_FORMAT_R8G8B8A8_SRGB)
							std::copy((uint32_t*)data.data(), ((uint32_t*)data.data())+w*h, image.begin());
						else if(imageInfo.format == VK_FORMAT_R16G16B16A16_SFLOAT)
							std::transform((uint64_t*)data.data(), ((uint64_t*)data.data())+w*h, image.begin(), [](uint64_t u){
								glm::vec4 v = glm::unpackHalf4x16(u);
								v = glm::clamp(v, glm::vec4(0.0), glm::vec4(1.0));
								return image_tools::color(v);
							});
						else if(imageInfo.format == VK_FORMAT_R16G16_SFLOAT)
							std::transform((uint32_t*)data.data(), ((uint32_t*)data.data())+w*h, image.begin(), [](uint64_t u){
								glm::vec2 v = glm::unpackHalf2x16(u);
								v = glm::clamp(v, glm::vec2(0.0), glm::vec2(1.0));
								return image_tools::color(glm::vec4(v, 0.0, 1.0));
							});
						else if(imageInfo.format == VK_FORMAT_R16G16B16A16_UINT)
							std::transform((uint64_t*)data.data(), ((uint64_t*)data.data())+w*h, image.begin(), [](uint64_t u){
								glm::u16vec4 uv = glm::unpackUint4x16(u);
								glm::vec4 v = glm::vec4(
									((double)uv.r)/((double)UINT16_MAX),
									((double)uv.g)/((double)UINT16_MAX),
									((double)uv.b)/((double)UINT16_MAX),
									((double)uv.a)/((double)UINT16_MAX));
								v = glm::clamp(v, glm::vec4(0.0), glm::vec4(1.0));
								return image_tools::color(v);
							});

						stbi_write_png(filename.c_str(), w, h, 4, image, w*4);
						*::logger << logger::begin << std::dec << "Framebuffer #" << m_attachment << " of size " 
							<< imageInfo.extent.width << "x" << imageInfo.extent.height
							<< " and format " << vk::to_string((vk::Format)imageInfo.format)
							<< " was encoded and written to PNG file \"" 
							<< filename << "\"." << logger::end;
					}
					catch(const std::exception& ex)
					{
						*::logger << logger::begin << std::dec << logger::error << "Could not encode and write framebuffer to PNG file \"" 
							<< filename << "\": " << ex.what() << logger::end;
					}
				}
				else {
#endif
				std::string filename = global_config["dumpDirectory"]+"/images/framebuffers/"+name+".image";
				try
				{
					std::ofstream of(filename, std::ios::binary);
					if(!of.good())
						throw std::runtime_error("ofstream is not good");
					of.write(data.data(), data.size());
					*::logger << logger::begin << std::dec << "Framebuffer #" << m_attachment <<" of size " 
						<< imageInfo.extent.width << "x" << imageInfo.extent.height << " (" << size 
						<< " bytes) and format " << vk::to_string((vk::Format)imageInfo.format)
						<< " was written to file \"" << filename << "\"." << logger::end;
				}
				catch(const std::exception& ex)
				{
					*::logger << logger::begin << std::dec << logger::error << "Could not write framebuffer of size " 
						<< size << " to file \"" << filename << "\": " << ex.what() << logger::end;
				}
#ifdef USE_IMAGE_TOOLS
				}
#endif
			}
		});
		rule_env.threads.push_back(std::move(thread));
	}

	void dump_framebuffer_action::read(std::istream& in)
	{
		in >> m_attachment;
		skip_ws(in);
		check_stream(in, ')');
	}

	std::ostream& dump_framebuffer_action::print(std::ostream& out)
	{
		out << "dumpfb(" << m_attachment << ")";
		return out;
	}

	void every_action::execute(selector_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(m_i == 0)
			m_action->execute(type, handle, ctx, rule);
		
		m_i++;
		m_i %= m_n;
	}

	void every_action::read(std::istream& in)
	{
		in >> m_n;
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		m_action = read_action(in, m_type);
		check_stream(in, ')');
	}

	std::ostream& every_action::print(std::ostream& out)
	{
		out << "every(" << m_n << ", ";
		m_action->print(out);
		out << ")";
		return out;
	}

	void buffer_copy_action::execute(selector_type stype, VkHandle handle, local_context& ctx, rule& rule)
	{
		VkHandle src = std::get<VkHandle>(m_src->get(stype, data_type::Handle, handle, ctx, rule));
		VkHandle dst = std::get<VkHandle>(m_dst->get(stype, data_type::Handle, handle, ctx, rule));
		
		std::vector<uint8_t> transferBuffer(m_size);

		memoryAccess(ctx.device, (VkBuffer)src, [&transferBuffer, this, &ctx](void* ptr, VkDeviceSize size){
			uint8_t* uptr = (uint8_t*)ptr;
			ctx.logger << "Reading " << std::min(m_size, size) << " bytes! ";
			std::copy(uptr, uptr + std::min(m_size, size), transferBuffer.begin());
		}, m_srcOffset);
		memoryAccess(ctx.device, (VkBuffer)dst, [&transferBuffer, this, &ctx](void* ptr, VkDeviceSize size){
			uint8_t* uptr = (uint8_t*)ptr;
			ctx.logger << "Writing " << std::min(m_size, size) << " bytes! ";
			std::copy(transferBuffer.begin(), transferBuffer.begin() + std::min(m_size, size), uptr);
		}, m_dstOffset);
	}

	void buffer_copy_action::read(std::istream& in)
	{
		m_src = read_data(in, m_type);
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		m_dst = read_data(in, m_type);
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		in >> m_srcOffset;
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		in >> m_dstOffset;
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		in >> m_size;
		skip_ws(in);
		check_stream(in, ')');
	}

	std::ostream& buffer_copy_action::print(std::ostream& out)
	{
		out << "buffer_copy(";
		m_src->print(out);
		out << ", ";
		m_dst->print(out);
		out << ", " << m_srcOffset << ", " << m_dstOffset << ", " << m_size << ")";
		return out;
	}

	void set_global_action::execute(selector_type stype, VkHandle handle, local_context& ctx, rule& rule)
	{
		rule_env.global_variables[m_name] = m_data->get(stype, m_dtype, handle, ctx, rule);
	}

	void set_global_action::read(std::istream& in)
	{
		std::string dtype;
		std::getline(in, dtype, ',');
		skip_ws(in);
		m_dtype = data_type_from_string(dtype);

		std::getline(in, m_name, ',');
		skip_ws(in);

		m_data = read_data(in, m_type);
		if(!m_data->supports(m_type, m_dtype))
		{
			std::ostringstream of;
			of << "Data \"";
			m_data->print(of);
			of << "\" does not support type " << to_string(m_dtype) << " for selector " << to_string(m_type) << ".";
			throw RULE_ERROR(of.str());
		}

		skip_ws(in);
		check_stream(in, ')');
	}

	std::ostream& set_global_action::print(std::ostream& out)
	{
		out << "set_global(" << to_string(m_dtype) << ", " << m_name << ", ";
		m_data->print(out);
		return out << ")";
	}

	void set_local_action::execute(selector_type stype, VkHandle handle, local_context& ctx, rule& rule)
	{
		ctx.local_variables[m_name] = m_data->get(stype, m_dtype, handle, ctx, rule);
	}

	void set_local_action::read(std::istream& in)
	{
		std::string dtype;
		std::getline(in, dtype, ',');
		skip_ws(in);
		m_dtype = data_type_from_string(dtype);

		std::getline(in, m_name, ',');
		skip_ws(in);

		m_data = read_data(in, m_type);
		if(!m_data->supports(m_type, m_dtype))
		{
			std::ostringstream of;
			of << "Data \"";
			m_data->print(of);
			of << "\" does not support type " << to_string(m_dtype) << " for selector " << to_string(m_type) << ".";
			throw RULE_ERROR(of.str());
		}

		skip_ws(in);
		check_stream(in, ')');
	}

	std::ostream& set_local_action::print(std::ostream& out)
	{
		out << "set_local(" << to_string(m_dtype) << ", " << m_name << ", ";
		m_data->print(out);
		return out << ")";
	}
}
