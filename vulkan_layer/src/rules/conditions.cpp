#include "rules/conditions.hpp"
#include "rules/rules.hpp"
#include "rules/execution_env.hpp"
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <istream>

namespace CheekyLayer
{
	condition_register<hash_condition> hash_condition::reg("hash");
	condition_register<mark_condition> mark_condition::reg("mark");
	condition_register<with_condition> with_condition::reg("with");
	condition_register<not_condition> not_condition::reg("not");
	condition_register<or_condition> or_condition::reg("or");

	bool hash_condition::test(selector_type stype, VkHandle handle, local_context&)
	{
		auto p = rule_env.hashes.find(handle);
		if(p == rule_env.hashes.end())
			return false;
		return p->second == m_hash;
	}

	void hash_condition::read(std::istream& in)
	{
		std::getline(in, m_hash, ')');
	}

	std::ostream& hash_condition::print(std::ostream& out)
	{
		out << "hash(" << m_hash << ")";
		return out;
	}

	bool mark_condition::test(selector_type stype, VkHandle handle, local_context&)
	{
		auto p = rule_env.marks.find(handle);
		if(p == rule_env.marks.end())
			return false;
		return std::find(p->second.begin(), p->second.end(), m_mark) != p->second.end();
	}

	void mark_condition::read(std::istream& in)
	{
		std::getline(in, m_mark, ')');
	}

	std::ostream& mark_condition::print(std::ostream& out)
	{
		out << "mark(" << m_mark << ")";
		return out;
	}

	bool with_condition::test(selector_type stype, VkHandle handle, local_context& ctx)
	{
		std::vector<std::pair<selector_type, VkHandle>> list;
		if(stype == selector_type::Draw)
		{
			std::transform(ctx.info->draw.images.begin(), ctx.info->draw.images.end(), std::back_inserter(list), [](auto h){
				return std::pair{selector_type::Image, h};
			});
			std::transform(ctx.info->draw.vertexBuffers.begin(), ctx.info->draw.vertexBuffers.end(), std::back_inserter(list), [](auto h){
				return std::pair{selector_type::Buffer, h};
			});
			std::transform(ctx.info->draw.shaders.begin(), ctx.info->draw.shaders.end(), std::back_inserter(list), [](auto h){
				return std::pair{selector_type::Shader, h};
			});
			list.push_back({selector_type::Buffer, ctx.info->draw.indexBuffer});
		}
		if(stype == selector_type::Pipeline)
		{
			std::transform(ctx.info->pipeline.shaderStages.begin(), ctx.info->pipeline.shaderStages.end(), std::back_inserter(list), [](auto h){
				return std::pair{selector_type::Shader, h};
			});
		}

		for(auto [stype2, handle2] : list)
		{
			if(m_selector->test(stype2, handle2, ctx))
				return true;
		}

		return false;
	}

	void with_condition::read(std::istream& in)
	{
		m_selector = std::make_unique<selector>();
		in >> *m_selector;

		char end = in.get();
		if(end != ')')
			throw std::runtime_error("expected ')', but got '"+std::string(1, end)+"' instead");
	}

	std::ostream& with_condition::print(std::ostream& out)
	{
		out << "with(";
		m_selector->print(out);
		out << ")";

		return out;
	}

	bool not_condition::test(selector_type type, VkHandle handle, local_context& ctx)
	{
		return !m_condition->test(type, handle, ctx);
	}

	void not_condition::read(std::istream& in)
	{
		m_condition = read_condition(in, m_type);
		skip_ws(in);
		check_stream(in, ')');
	}

	std::ostream& not_condition::print(std::ostream& out)
	{
		out << "not(";
		m_condition->print(out);
		out << ")";
		return out;
	}

	bool or_condition::test(selector_type type, VkHandle handle, local_context& ctx)
	{
		for(auto& c : m_conditions)
		{
			if(c->test(type, handle, ctx))
				return true;
		}
		return false;
	}

	void or_condition::read(std::istream& in)
	{
		while(in.peek() != ')')
		{
			m_conditions.push_back(read_condition(in, m_type));

			skip_ws(in);
			char seperator = in.peek();
			if(seperator != ',')
				continue;
			in.get();
			skip_ws(in);
		}

		check_stream(in, ')');
	}

	std::ostream& or_condition::print(std::ostream& out)
	{
		out << "or(";
		for(int i=0; i<m_conditions.size(); i++)
		{
			m_conditions[i]->print(out);
			out << (i==m_conditions.size()-1 ? ")" : ", ");
		}
		return out;
	}
}
