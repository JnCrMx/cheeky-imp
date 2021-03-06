#pragma once

#include "rules.hpp"
#include "rules/execution_env.hpp"
#include <cstdint>
#include <istream>
#include <memory>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace CheekyLayer::rules::actions
{
	/** Marks a handle with the given string.
	 * A handle can have multiple marks at the same time,
	 * hence this actions appends a mark to the handle's list
	 * of marks.
	 *
	 * \par Usage
	 * \code{.unparsed}
     * mark(<mark>)
     * \endcode
	 * \param <mark> The string to make the handle with. Must not contain ')'.
	 * 
	 * \par Example
	 * This rule marks any image with the \ref conditions::hash_condition "hash" "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
	 * with the string "my-image".
	 * \code{.unparsed}
	 * image{hash(e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)} -> mark(my-image)
	 * \endcode
	 */
	class mark_action : public action
	{
		public:
			mark_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_mark;

			static action_register<mark_action> reg;
	};

	/** Removes one mark or all marks from a handle.
	 * 
	 * \par Usage
	 * \code{.unparsed}
	 * unmark(*)
	 * unmark(<mark>)
	 * \endcode
	 * 1. Removes all marks from the handle.
	 * 2. Removes on mark from the handle.
	 * \param <mark> The mark to remove from the handle. Must not contain ')'.
	 * 
	 * \par Example
	 * This rule removes the \ref actions::mark_action "mark" "hello" from all images \ref conditions::mark_condition "marked" with it.
	 * \code{.unparsed}
	 * image{mark(hello)} -> unmark(hello)
	 * \endcode
	 */
	class unmark_action : public action
	{
		public:
			unmark_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_mark;
			bool m_clear = false;

			static action_register<unmark_action> reg;
	};

	class verbose_action : public action
	{
		public:
			verbose_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:

			static action_register<verbose_action> reg;
	};

	class sequence_action : public action
	{
		public:
			sequence_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::vector<std::unique_ptr<action>> m_actions;

			static action_register<sequence_action> reg;
	};

	class each_action : public action
	{
		public:
			each_action(selector_type type) : action(type) {
				if(type != selector_type::Draw)
					throw std::runtime_error("the \"each\" action is only supported for draw selectors, but not for "+to_string(type)+" selectors");
			}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::unique_ptr<selector> m_selector;
			std::unique_ptr<action> m_action;

			static action_register<each_action> reg;
	};

	class on_action : public action
	{
		enum on_action_event
		{
			EndCommandBuffer,
			QueueSubmit,
			EndRenderPass
		};

		public:
			on_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		protected:
			on_action_event on_action_event_from_string(std::string s)
			{
				if(s=="EndCommandBuffer")
					return EndCommandBuffer;
				if(s=="QueueSubmit")
					return QueueSubmit;
				if(s=="EndRenderPass")
					return EndRenderPass;
				throw std::runtime_error("unsupported on_action_event: "+s);
			}

			std::string on_action_event_to_string(on_action_event e)
			{
				switch(e)
				{
					case EndCommandBuffer:
						return "EndCommandBuffer";
					case QueueSubmit:
						return "QueueSubmit";
					case EndRenderPass:
						return "EndRenderPass";
					default:
						return "unknown";
				}
			}
		private:
			on_action_event m_event;
			std::unique_ptr<action> m_action;

			static action_register<on_action> reg;
	};

	class disable_action : public action
	{
		public:
			disable_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:

			static action_register<disable_action> reg;
	};

	class cancel_action : public action
	{
		public:
			cancel_action(selector_type type) : action(type) {
				if(type != selector_type::Draw)
					throw std::runtime_error("the \"cancel\" action is only supported for draw selectors, but not for "+to_string(type)+" selectors");
			}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:

			static action_register<cancel_action> reg;
	};

	class log_action : public action
	{
		public:
			log_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_text;

			static action_register<log_action> reg;
	};

	class log_extended_action : public action
	{
		public:
			log_extended_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::unique_ptr<data> m_data;

			static action_register<log_extended_action> reg;
	};

	class override_action : public action
	{
		public:
			override_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_expression;

			static action_register<override_action> reg;
	};

	class socket_action : public action
	{
		public:
			socket_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		protected:
			
		private:
			std::string m_name;
			ipc::socket::socket_type m_socketType;
			std::string m_host;
			int m_port;
			ipc::socket::protocol_type m_protocol;

			static action_register<socket_action> reg;
	};

	class write_action : public action
	{
		public:
			write_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_fd;
			std::unique_ptr<data> m_data;

			static action_register<write_action> reg;
	};

	class store_action : public action
	{
		public:
			store_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:
			std::string m_name;

			static action_register<store_action> reg;
	};

	class overload_action : public action
	{
		enum mode {
			File,
			FileFromData,
			Data
		};

		public:
			overload_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);

			void workTry(std::string optFilename, std::vector<uint8_t> optData);
			void work(std::string optFilename, std::vector<uint8_t> optData);
		private:
			std::string m_target;
			mode m_mode;
			std::string m_filename;
			std::unique_ptr<data> m_data;

			static action_register<overload_action> reg;
	};

	class preload_action : public action
	{
		public:
			preload_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);

			void work(std::string optFilename);
		private:
			std::string m_target;
			std::unique_ptr<data> m_filename;

			static action_register<preload_action> reg;
	};

	class dump_framebuffer_action : public action
	{
		public:
			dump_framebuffer_action(selector_type type) : action(type) {
				if(type != selector_type::Draw)
					throw std::runtime_error("the \"dumpfb\" action is only supported for draw selectors, but not for "+to_string(type)+" selectors");
			}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:
			int m_attachment;

			static action_register<dump_framebuffer_action> reg;
	};

	class every_action : public action
	{
		public:
			every_action(selector_type type) : action(type) {}
			virtual void read(std::istream&);
			virtual void execute(selector_type, VkHandle, local_context&, rule&);
			virtual std::ostream& print(std::ostream&);
		private:
			int m_i = 0;
			int m_n;
			std::unique_ptr<action> m_action;

			static action_register<every_action> reg;
	};
}
