#include "rules/actions.hpp"

#include "draw.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "objects.hpp"
#include "utils.hpp"

#include <condition_variable>
#include <experimental/iterator>
#include <ranges>
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
	action_register<server_socket_action> server_socket_action::reg("server_socket");
	action_register<write_action> write_action::reg("write");
	action_register<load_image_action> load_image_action::reg("load_image");
	action_register<preload_image_action> preload_image_action::reg("preload_image");
	action_register<dump_framebuffer_action> dump_framebuffer_action::reg("dumbfb");
	action_register<every_action> every_action::reg("every");
	action_register<buffer_copy_action> buffer_copy_action::reg("buffer_copy");
	action_register<set_global_action> set_global_action::reg("set_global");
	action_register<set_global_action> set_global_action::reg2("global=");
	action_register<set_local_action> set_local_action::reg("set_local");
	action_register<set_local_action> set_local_action::reg2("local=");
	action_register<thread_action> thread_action::reg("thread");
	action_register<define_function_action> define_function_action::reg("function");

	void mark_action::execute(selector_type type, VkHandle handle, global_context& global, local_context& local, rule&)
	{
		global.marks[(VkHandle)handle].emplace(m_mark);
		local.logger.info("Marked {} {} as \"{}\"", to_string(type), handle, m_mark);
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

	void unmark_action::execute(selector_type type, VkHandle handle, global_context& global, local_context& local, rule&)
	{
		if(m_clear)
		{
			global.marks[(VkHandle)handle].clear();
			local.logger.info("Cleared marks of {} {}", to_string(type), handle);
		}
		else
		{
			auto& marks = global.marks[(VkHandle)handle];
			if(marks.contains(m_mark))
			{
				marks.erase(m_mark);
				local.logger.info("Unmarked {} {} as \"{}\"", to_string(type), handle, m_mark);
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

	void verbose_action::execute(selector_type, VkHandle, global_context&, local_context& local, rule&)
	{
		if(local.printVerbose)
			local.printVerbose(local.logger);
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

	void sequence_action::execute(selector_type type, VkHandle handle, global_context& global, local_context& local, rule& rule)
	{
		for(auto& a : m_actions)
		{
			a->execute(type, handle, global, local, rule);
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

	void each_action::execute(selector_type type, VkHandle handle, global_context& global, local_context& local, rule& rule)
	{
		std::vector<VkHandle> handles;
		if(type == selector_type::Draw)
		{
			auto& info = std::get<draw_info>(*local.info);
			switch(m_selector->get_type())
			{
				case selector_type::Image:
					std::ranges::copy(info.images, std::back_inserter(handles));
					break;
				case selector_type::Shader:
					std::ranges::copy(info.shaders, std::back_inserter(handles));
					break;
				case selector_type::Buffer:
					std::ranges::copy(info.vertexBuffers, std::back_inserter(handles));
					handles.push_back(info.indexBuffer);
					break;
				default:
					throw RULE_ERROR("unsupported selector type: "+std::to_string(m_selector->get_type()));
			}
		}
		for(auto h : handles)
		{
			if(m_selector->test(m_selector->get_type(), h, global, local))
				m_action->execute(m_selector->get_type(), h, global, local, rule);
		}
	}

	void each_action::read(std::istream& in)
	{
		m_selector = std::make_unique<selector>();
		in >> *m_selector;

		selector_type type = m_selector->get_type();
		if(type != selector_type::Image && type != selector_type::Buffer && type != selector_type::Shader)
			throw RULE_ERROR("unsupported selector type "+to_string(type)+" only image, buffer and shader selectors are supported");

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

	void on_action::execute(selector_type type, VkHandle handle, global_context& global, local_context& local, rule& rule)
	{
		if(!local.commandBuffer)
		{
			local.logger.error("Cannot schedule action, because CommandBuffer is not known. We will execute it right now instead.");
			m_action->execute(type, handle, global, local, rule);
			return;
		}

		const auto locals = local.local_variables;
		local.logger.debug("Saving locals: {}", fmt::join(locals | std::ranges::views::keys, ", "));

		switch(m_event)
		{
			case EndCommandBuffer:
				global.onEndCommandBuffer(local.commandBuffer, [this, type, handle, &rule, locals, &global](local_context& ctx2){
					local_context deref = ctx2;
					deref.local_variables = locals;
					this->m_action->execute(type, handle, global, deref, rule);
				});
				break;
			case QueueSubmit:
				global.onQueueSubmit(local.commandBuffer, [this, type, handle, &rule, locals, &global](local_context& ctx2){
					local_context deref = ctx2;
					deref.local_variables = locals;
					this->m_action->execute(type, handle, global, deref, rule);
				});
				break;
			case EndRenderPass:
				global.onEndRenderPass(local.commandBuffer, [this, type, handle, &rule, locals, &global](local_context& ctx2){
					local_context deref = ctx2;
					deref.local_variables = locals;
					this->m_action->execute(type, handle, global, deref, rule);
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

	void disable_action::execute(selector_type, VkHandle, global_context&, local_context&, rule& rule)
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

	void cancel_action::execute(selector_type, VkHandle, global_context&, local_context& local, rule&)
	{
		local.canceled = true;
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

	void log_action::execute(selector_type stype, VkHandle handle, global_context& global, local_context& local, rule&)
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
			auto p = global.marks.find(handle);
			if(p != global.marks.end())
			{
				auto& v = p->second;

				std::stringstream oss;
				oss << "[";
				auto it = v.cbegin();
				for(int i=0; it != v.cend(); i++, it++)
				{
					replace(s, "$marks["+std::to_string(i)+"]", *it);
					oss << *it << (i == v.size()-1 ? "]" : ",");
				}
				replace(s, "$marks[*]", oss.str());
			}
		}

		local.logger.info("{}", s);
	}

	void log_action::read(std::istream& in)
	{
		in >> std::quoted(m_text);
		skip_ws(in);
		check_stream(in, ')');
	}

	std::ostream& log_action::print(std::ostream& out)
	{
		out << "log(" << std::quoted(m_text) << ")";
		return out;
	}

	void log_extended_action::execute(selector_type stype, VkHandle handle, global_context& global, local_context& local, rule& rule)
	{
		local.logger.info("{}",
			std::get<std::string>(m_data->get(stype, data_type::String, handle, global, local, rule))
		);
	}

	void log_extended_action::read(std::istream& in)
	{
		m_data = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_data->supports(m_type, data_type::String))
			throw RULE_ERROR("data does not support strings");
	}

	std::ostream& log_extended_action::print(std::ostream& out)
	{
		out << "logx(";
		m_data->print(out);
		out << ")";
		return out;
	}

	void override_action::execute(selector_type, VkHandle, global_context&, local_context& local, rule &)
	{
		local.overrides.push_back(m_expression);
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

	void socket_action::execute(selector_type, VkHandle, global_context& global, local_context& local, rule &)
	{
		if(global.fds.contains(m_name))
		{
			if(auto* socket = dynamic_cast<ipc::socket*>(global.fds[m_name].get()))
			{
				socket->close(); // it is now allowed to reopen sockets, but this doesn't work yet and causes a crash
			}
			else
			{
				throw RULE_ERROR("file descriptor with name "+m_name+" already exists and is not a socket");
			}
		}

		global.fds[m_name] = std::make_unique<ipc::socket>(m_socketType, m_host, m_port, m_protocol, local.instance, local.device);
		global.fds[m_name]->m_name = m_name;
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

	void server_socket_action::execute(selector_type, VkHandle, global_context& global, local_context& local, rule &)
	{
		if(global.fds.contains(m_name))
			throw RULE_ERROR("file descriptor with name "+m_name+" already exists");

		global.fds[m_name] = std::make_unique<ipc::server_socket>(m_socketType, m_host, m_port, m_protocol, local.instance, local.device);
		global.fds[m_name]->m_name = m_name;
	}

	void server_socket_action::read(std::istream& in)
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

	std::ostream& server_socket_action::print(std::ostream& out)
	{
		out << "server_socket(" << m_name << ", " << ipc::socket::socket_type_to_string(m_socketType) << ", " << m_host << ", " << m_port
			<< ", " << ipc::socket::protocol_type_to_string(m_protocol) << ")";
		return out;
	}

	void write_action::execute(selector_type stype, VkHandle handle, global_context& global, local_context& local, rule& rule)
	{
		if(!global.fds.contains(m_fd))
			throw RULE_ERROR("file descriptor with name "+m_fd+" does not exists");

		auto& fd = global.fds[m_fd];

		int extra = 0;
		if(stype == Receive || (stype == Custom && local.customTag == "connect"))
		{
			extra = std::get<receive_info>(*local.info).extra;
		}

		if(m_data->supports(stype, data_type::Raw))
		{
			data_value val = m_data->get(stype, data_type::Raw, handle, global, local, rule);
			auto& v = std::get<std::vector<uint8_t>>(val);
			if(fd->write(v, extra) < 0)
				throw RULE_ERROR("failed to send data to file descriptor \""+m_fd+"\": " + strerror(errno));
		}
		else if(m_data->supports(stype, data_type::String))
		{
			data_value val = m_data->get(stype, data_type::String, handle, global, local, rule);
			auto& s = std::get<std::string>(val);
			std::vector<uint8_t> v(s.begin(), s.end());
			if(fd->write(v, extra) < 0)
				throw RULE_ERROR("failed to write data to file descriptor \""+m_fd+"\": " + strerror(errno));
		}
		else
			throw RULE_ERROR("no suitable data type available");
	}

	void write_action::read(std::istream& in)
	{
		std::getline(in, m_fd, ',');
		skip_ws(in);

		m_data = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_data->supports(m_type, data_type::String) && !m_data->supports(m_type, data_type::Raw))
			throw RULE_ERROR("data does not support strings or raw data");
	}

	std::ostream& write_action::print(std::ostream& out)
	{
		out << "write(" << m_fd << ", ";
		m_data->print(out);
		out << ")";
		return out;
	}

	void load_image_action::execute(selector_type stype, VkHandle handle, global_context& global, local_context& local, rule& rule)
	{
		VkHandle h = std::get<VkHandle>(m_target->get(stype, data_type::Handle, handle, global, local, rule));

		scoped_lock l(global_lock);
		if(m_mode == mode::Data)
		{
			std::vector<uint8_t> data = std::get<std::vector<uint8_t>>(m_data->get(stype, data_type::Raw, handle, global, local, rule));
			std::thread t(&load_image_action::workTry, this, std::ref(*local.device), h, std::string{}, data);
			global.threads.push_back(std::move(t));
		}
		else if(m_mode == mode::FileFromData)
		{
			std::string filename = std::get<std::string>(m_data->get(stype, data_type::String, handle, global, local, rule));
			std::thread t(&load_image_action::workTry, this, std::ref(*local.device), h, filename, std::vector<uint8_t>{});
			global.threads.push_back(std::move(t));
		}
		else
		{
			std::thread t(&load_image_action::workTry, this, std::ref(*local.device), h, m_filename, std::vector<uint8_t>{});
			global.threads.push_back(std::move(t));
		}
	}

	void load_image_action::workTry(device& device, VkHandle handle, std::string optFilename, std::vector<uint8_t> optData)
	{
		try
		{
			work(device, handle, optFilename, std::move(optData));
		}
		catch(const std::exception& ex)
		{
			device.logger->error("overload image failed: {}", ex.what());
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

		auto operator<=>(const image_key&) const = default;
	};
	std::map<image_key, std::pair<std::vector<uint8_t>, std::vector<VkDeviceSize>>> imageFileCache;
	void load_image_action::work(device& device, VkHandle handle, std::string optFilename, std::vector<uint8_t> optData)
	{
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
			if(optFilename.ends_with(".png") && imageTools)
			{
				VkImageCreateInfo imgInfo = device.images.at((VkImage)handle).createInfo;
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
			else if(optFilename.find("${mip}") != std::string::npos)
			{
				VkImageCreateInfo imgInfo = device.images.at((VkImage)handle).createInfo;
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
		if((result = device.dispatch.CreateBuffer(*device, &bufferInfo, nullptr, &buffer)) != VK_SUCCESS)
			throw RULE_ERROR("failed to create staging buffer: "+vk::to_string((vk::Result)result));

		VkMemoryRequirements requirements;
		device.dispatch.GetBufferMemoryRequirements(*device, buffer, &requirements);

		VkMemoryAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		allocateInfo.memoryTypeIndex = findMemoryType(device.memProperties, requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		allocateInfo.allocationSize = requirements.size;
		VkDeviceMemory memory;
		if(device.dispatch.AllocateMemory(*device, &allocateInfo, nullptr, &memory) != VK_SUCCESS)
			throw RULE_ERROR("failed to allocate memory for staging buffer");

		if(device.dispatch.BindBufferMemory(*device, buffer, memory, 0) != VK_SUCCESS)
			throw RULE_ERROR("failed to bind memory to staging buffer");

		void* bufferPointer;
		if(device.dispatch.MapMemory(*device, memory, 0, data.size(), 0, &bufferPointer) != VK_SUCCESS)
			throw RULE_ERROR("failed to map staging memory");
		memcpy(bufferPointer, data.data(), data.size());
		device.dispatch.UnmapMemory(*device, memory);

		auto tStagingReady = std::chrono::high_resolution_clock::now();

		{
			scoped_lock l(transfer_lock);

			VkCommandBuffer commandBuffer = device.transferBuffer;
			if(commandBuffer == VK_NULL_HANDLE)
				throw RULE_ERROR("command buffer is NULL");

			if(device.dispatch.ResetCommandBuffer(commandBuffer, 0) != VK_SUCCESS)
				throw RULE_ERROR("failed to reset command buffer");

			VkCommandBufferBeginInfo beginInfo;
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.pNext = nullptr;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			beginInfo.pInheritanceInfo = nullptr;
			if(device.dispatch.BeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
				throw RULE_ERROR("failed to begin the command buffer");

			{
				VkImageCreateInfo imgInfo = device.images.at((VkImage)handle).createInfo;

				VkImageMemoryBarrier memoryBarrier{};
				memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				memoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				memoryBarrier.image = (VkImage) handle;
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

				device.dispatch.CmdPipelineBarrier(commandBuffer,
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
				device.dispatch.CmdCopyBufferToImage(commandBuffer, buffer, (VkImage)handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					copies.size(), copies.data());
			}

			if(device.dispatch.EndCommandBuffer(commandBuffer) != VK_SUCCESS)
				throw RULE_ERROR("failed to end command buffer");

			auto tCommandReady = std::chrono::high_resolution_clock::now();

			VkQueue queue = device.transferQueue;
			if(queue == VK_NULL_HANDLE)
				throw RULE_ERROR("command buffer is NULL");

			if(device.dispatch.QueueWaitIdle(queue) != VK_SUCCESS)
				throw RULE_ERROR("cannot wait for queue to be idle before copying");
			auto tIdle1 = std::chrono::high_resolution_clock::now();

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;
			if(device.dispatch.QueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
				throw RULE_ERROR("failed to submit command buffer");

			if(device.dispatch.QueueWaitIdle(queue) != VK_SUCCESS)
				throw RULE_ERROR("cannot wait for queue to be idle after copying");
			auto tIdle2 = std::chrono::high_resolution_clock::now();

			device.dispatch.DestroyBuffer(*device, buffer, nullptr);
			device.dispatch.FreeMemory(*device, memory, nullptr);

			device.logger->debug(
R"(overload timing:
	image load: {}
	staging buffer: {}
	command buffer: {}
	queue wait: {}
	copy operation: {})",
				std::chrono::duration_cast<std::chrono::milliseconds>(tDataReady - t0).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(tStagingReady - tDataReady).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(tCommandReady - tStagingReady).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(tIdle1 - tCommandReady).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(tIdle2 - tIdle1).count());
		}
	}

	void load_image_action::read(std::istream& in)
	{
		m_target = read_data(in, m_type);
		if(!m_target->supports(m_type, data_type::Handle))
			throw RULE_ERROR("target data does not support handle data type");
		check_stream(in, ',');
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
				throw RULE_ERROR("data does not support string data");
		}
		else if(mode == "Data")
		{
			m_mode = mode::Data;

			m_data = read_data(in, m_type);
			check_stream(in, ')');

			if(!m_data->supports(m_type, data_type::Raw))
				throw RULE_ERROR("data does not support raw data");
		}
		else
			throw RULE_ERROR("mode "+mode+" is not supported, must be either File, FileFromData or Data");
	}

	std::ostream& load_image_action::print(std::ostream& out)
	{
		out << "load_image(";
		m_target->print(out);
		out << ", ";
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

	void preload_image_action::execute(selector_type type, VkHandle handle, global_context& global, local_context& local, rule& rule)
	{
		std::string filename = std::get<std::string>(m_filename->get(type, data_type::String, handle, global, local, rule));
		VkHandle h = std::get<VkHandle>(m_target->get(type, data_type::Handle, handle, global, local, rule));
		std::thread t(&preload_image_action::work, this, std::ref(*local.device), h, filename);
		global.threads.push_back(std::move(t));
	}

	void preload_image_action::work(device& device, VkHandle handle, std::string optFilename)
	{
		try
		{
			if(optFilename.ends_with(".png") && imageTools)
			{
				VkImageCreateInfo imgInfo = device.images.at((VkImage)handle).createInfo;
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
			device.logger->error("preload image failed: {}", ex.what());
		}
	}

	void preload_image_action::read(std::istream& in)
	{
		m_target = read_data(in, m_type);
		if(!m_target->supports(m_type, data_type::Handle))
			throw RULE_ERROR("target data does not support handle data type");
		check_stream(in, ',');
		skip_ws(in);

		m_filename = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_filename->supports(m_type, data_type::String))
			throw RULE_ERROR("data does not support string data");
	}

	std::ostream& preload_image_action::print(std::ostream& out)
	{
		out << "preload_image(";
		m_target->print(out);
		out << ", ";
		m_filename->print(out);
		out << ")";
		return out;
	}

	void dump_framebuffer_action::execute(selector_type, VkHandle, global_context& global, local_context &local, rule&)
	{
		auto& device = *local.device;

		VkBuffer buffer;
		VkDeviceMemory memory;
		VkEvent event;

		VkFramebuffer fb = local.commandBufferState->framebuffer;
		VkImageView view = device.framebuffers[fb].attachments.at(m_attachment);
		VkImage image = device.imageViewToImage[view];
		VkImageCreateInfo imageInfo = device.images.at(image).createInfo;

		VkMemoryRequirements req;
		device.dispatch.GetImageMemoryRequirements(*device, image, &req);
		VkDeviceSize size = req.size;

		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = size;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		device.dispatch.CreateBuffer(*device, &bufferCreateInfo, nullptr, &buffer);

		device.dispatch.GetBufferMemoryRequirements(*device, buffer, &req);

		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = req.size;
		allocateInfo.memoryTypeIndex = findMemoryType(device.memProperties,
			req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		device.dispatch.AllocateMemory(*device, &allocateInfo, nullptr, &memory);

		device.dispatch.BindBufferMemory(*device, buffer, memory, 0);

		VkEventCreateInfo eventCreateInfo{};
		eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
		device.dispatch.CreateEvent(*device, &eventCreateInfo, nullptr, &event);

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

		device.dispatch.CmdPipelineBarrier(local.commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr, 0, nullptr, 1, &barrier);
		device.dispatch.CmdCopyImageToBuffer(local.commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
		device.dispatch.CmdSetEvent(local.commandBuffer, event, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

		std::thread thread([this, &device, imageInfo, size, buffer, memory, event](){
			for(;;)
			{
				if(device.dispatch.GetEventStatus(*device, event) == VK_EVENT_SET)
					break;
				using std::chrono::operator""ms;
				std::this_thread::sleep_for(10ms);
			}

			{
				std::vector<char> data(size);

				{
					scoped_lock l(global_lock);
					char* p;
					device.dispatch.MapMemory(*device, memory, 0, size, 0, (void**)&p);
					std::copy(p, p+size, data.begin());
					device.dispatch.UnmapMemory(*device, memory);

					device.dispatch.DestroyBuffer(*device, buffer, nullptr);
					device.dispatch.FreeMemory(*device, memory, nullptr);
					device.dispatch.DestroyEvent(*device, event, nullptr);
				}

				auto t = std::chrono::high_resolution_clock::now();
				std::chrono::duration<long> seconds = std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch());

				std::string name = std::to_string(seconds.count()) + "_" + std::to_string(m_attachment);

#ifdef USE_IMAGE_TOOLS
				bool decompression = image_tools::is_decompression_supported(imageInfo.format);
				if(decompression ||
					imageInfo.format == VK_FORMAT_R8G8B8A8_SRGB ||
					imageInfo.format == VK_FORMAT_R8G8B8A8_UNORM ||
					imageInfo.format == VK_FORMAT_R16G16B16A16_SFLOAT ||
					imageInfo.format == VK_FORMAT_R16G16_SFLOAT ||
					imageInfo.format == VK_FORMAT_R16G16B16A16_UINT)
				{
					std::string filename = device.inst->config.dump_directory / "images/framebuffers" / (name+".png");
					try
					{
						int w = imageInfo.extent.width;
						int h = imageInfo.extent.height;

						image_tools::image image(w, h);

						if(decompression)
							image_tools::decompress(imageInfo.format, (uint8_t*)data.data(), image, w, h);
						else if(imageInfo.format == VK_FORMAT_R8G8B8A8_SRGB || imageInfo.format == VK_FORMAT_R8G8B8A8_UNORM)
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
						device.logger->info("Framebuffer #{} of size {}x{} and format {} was encoded and written to PNG file \"{}\".",
							m_attachment, imageInfo.extent.width, imageInfo.extent.height, vk::to_string((vk::Format)imageInfo.format), filename);
					}
					catch(const std::exception& ex)
					{
						device.logger->error("Could not encode and write framebuffer to PNG file \"{}\": {}", filename, ex.what());
					}
				}
				else {
#endif
				std::string filename = device.inst->config.dump_directory / "images/framebuffers" / (name+".image");
				try
				{
					std::ofstream of(filename, std::ios::binary);
					if(!of.good())
						throw RULE_ERROR("ofstream is not good");
					of.write(data.data(), data.size());
					device.logger->info("Framebuffer #{} of size {}x{} and format {} was written to file \"{}\".",
						m_attachment, imageInfo.extent.width, imageInfo.extent.height, vk::to_string((vk::Format)imageInfo.format), filename);
				}
				catch(const std::exception& ex)
				{
					device.logger->error("Could not write framebuffer of size {} to file \"{}\": {}",
						size, filename, ex.what());
				}
#ifdef USE_IMAGE_TOOLS
				}
#endif
			}
		});
		global.threads.push_back(std::move(thread));
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

	void every_action::execute(selector_type type, VkHandle handle, global_context& global, local_context& local, rule& rule)
	{
		if(m_i == 0)
			m_action->execute(type, handle, global, local, rule);

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

	void buffer_copy_action::execute(selector_type stype, VkHandle handle, global_context& global, local_context& local, rule& rule)
	{
		VkHandle srcHandle, dstHandle;
		VkDeviceSize srcOffset = 0, dstOffset = 0;
		if(m_src->supports(stype, data_type::List))
		{
			data_list src = std::get<data_list>(m_src->get(stype, data_type::List, handle, global, local, rule));
			srcHandle = std::get<VkHandle>(src.values.at(0));
			srcOffset = std::get<double>(src.values.at(1));
		}
		else
		{
			srcHandle = std::get<VkHandle>(m_src->get(stype, data_type::Handle, handle, global, local, rule));
		}
		if(m_dst->supports(stype, data_type::List))
		{
			data_list dst = std::get<data_list>(m_dst->get(stype, data_type::List, handle, global, local, rule));
			dstHandle = std::get<VkHandle>(dst.values.at(0));
			dstOffset = std::get<double>(dst.values.at(1));
		}
		else
		{
			dstHandle = std::get<VkHandle>(m_dst->get(stype, data_type::Handle, handle, global, local, rule));
		}

		std::vector<uint8_t> transferBuffer(m_size);

		local.device->memory_access((VkBuffer) srcHandle, [&transferBuffer, this, &local, srcHandle](void* ptr, VkDeviceSize size){
			uint8_t* uptr = (uint8_t*)ptr;
			local.logger.debug("Reading {} bytes from buffer {}", std::min(m_size, size), srcHandle);
			std::copy(uptr, uptr + std::min(m_size, size), transferBuffer.begin());
		}, srcOffset + m_srcOffset);
		local.device->memory_access((VkBuffer) dstHandle, [&transferBuffer, this, &local, dstHandle](void* ptr, VkDeviceSize size){
			uint8_t* uptr = (uint8_t*)ptr;
			local.logger.debug("Writing {} bytes to buffer {}", std::min(m_size, size), dstHandle);
			std::copy(transferBuffer.begin(), transferBuffer.begin() + std::min(m_size, size), uptr);
		}, dstOffset + m_dstOffset);
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

	void set_global_action::execute(selector_type stype, VkHandle handle, global_context& global, local_context& local, rule& rule)
	{
		global.global_variables[m_name] = m_data->get(stype, m_dtype, handle, global, local, rule);
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

	void set_local_action::execute(selector_type stype, VkHandle handle, global_context& global, local_context& local, rule& rule)
	{
		local.local_variables[m_name] = m_data->get(stype, m_dtype, handle, global, local, rule);
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

	void thread_action::execute(selector_type, VkHandle, global_context& global, local_context& local, rule& rule)
	{
		m_instance = local.instance;
		m_device = local.device;
		m_local_variables = local.local_variables;
		if(m_done) return;
		m_done = true;

		m_thread = std::jthread([this, &rule](std::stop_token stop_token){
			std::mutex mutex;
			std::unique_lock<std::mutex> lock(mutex);
			do
			{
				calling_context ctx = {
					.customTag = "thread",
					.local_variables = m_local_variables,
				};
				if(m_device) {
					m_device->execute_rules(rules::selector_type::Custom, VK_NULL_HANDLE, ctx);
				}
				else if(m_instance) {
					m_instance->execute_rules(rules::selector_type::Custom, VK_NULL_HANDLE, ctx);
				}
			}
			while(!std::condition_variable_any().wait_for(lock, stop_token, std::chrono::milliseconds(m_delay), [&stop_token]{return stop_token.stop_requested();}));
		});
	}

	void thread_action::read(std::istream& in)
	{
		m_action = read_action(in, m_type);
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		in >> m_delay;
		check_stream(in, ')');
	}

	std::ostream& thread_action::print(std::ostream& out)
	{
		out << "thread(";
		m_action->print(out);
		return out << ", " << m_delay << ")";
	}

	void define_function_action::read(std::istream& in)
	{
		std::getline(in, m_name, ',');
		check_stream(in, '(');

		std::string sig;
		std::getline(in, sig, ')');
		{
			std::istringstream iss(sig);
			skip_ws(iss);

			std::string t;
			while(std::getline(iss, t, ','))
			{
				skip_ws(iss);
				m_arguments.push_back(data_type_from_string(t));
			}
		}
		check_stream(in, ',');
		skip_ws(in);
		m_function = read_data(in, m_type); // TODO: FIX usage of m_type
		skip_ws(in);

		while(in.peek() != ')')
		{
			m_default_arguments.push_back(read_data(in, m_type)); // TODO: FIX usage of m_type

			skip_ws(in);
			char seperator = in.peek();
			if(seperator != ',')
				continue;
			in.get();
			skip_ws(in);
		}
		check_stream(in, ')');

		for(int i=0; i<m_default_arguments.size(); i++)
		{
			int index = i + m_arguments.size() - m_default_arguments.size();
			auto t = m_arguments.at(i);
			if(!m_default_arguments.at(i)->supports(m_type, t)) // TODO: FIX usage of m_type
				throw RULE_ERROR("default argument "+std::to_string(i)+" does not support type "+to_string(t));
		}
	}

	std::ostream& define_function_action::print(std::ostream& out)
	{
		out << "function(" << m_name << ", (";
		std::transform(m_arguments.begin(), m_arguments.end(), std::experimental::make_ostream_joiner(out, ", "),
			static_cast<std::string(*)(data_type)>(to_string));
		out << "), ";
		m_function->print(out);
		if(m_default_arguments.empty())
			return out << ")";
		for(auto& arg : m_default_arguments)
		{
			out << ", ";
			arg->print(out);
		}
		return out << ")";
	}

	void define_function_action::execute(selector_type, VkHandle, global_context& global, local_context &, rule &)
	{
		register_function(global);
	}

	void define_function_action::register_function(global_context& global)
	{
		user_function fnc = {
			.data = m_function.get(),
			.arguments = m_arguments,
			.default_arguments = std::vector<class data*>(m_default_arguments.size())
		};
		std::transform(m_default_arguments.cbegin(), m_default_arguments.cend(), fnc.default_arguments.begin(), std::mem_fn(&std::unique_ptr<data>::get));
		global.user_functions[m_name] = std::move(fnc);
	}
}
