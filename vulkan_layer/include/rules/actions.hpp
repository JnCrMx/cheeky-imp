#pragma once

#include "rules.hpp"
#include "rules/execution_env.hpp"
#include <istream>
#include <memory>
#include <stdexcept>

namespace CheekyLayer
{
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
			QueueSubmit
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
			socket_type socket_type_from_string(std::string s)
			{
				if(s=="TCP")
					return TCP;
				if(s=="UDP")
					return UDP;
				throw std::runtime_error("unsupported socket_type: "+s);
			}

			std::string socket_type_to_string(socket_type e)
			{
				switch(e)
				{
					case TCP:
						return "TCP";
					case UDP:
						return "UDP";
					default:
						return "unknown";
				}
			}
		private:
			std::string m_name;
			socket_type m_socketType;
			std::string m_host;
			int m_port;

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
}
