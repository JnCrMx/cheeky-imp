#include "descriptors.hpp"
#include "draw.hpp"
#include "rules/data.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "reflection/vkreflection.hpp"
#include "reflection/reflectionparser.hpp"

namespace CheekyLayer::rules::datas
{
	data_register<vkstruct_data> vkstruct_data::reg("vkstruct");
	data_register<vkdescriptor_data> vkdescriptor_data::reg("vkdescriptor");

	void vkstruct_data::read(std::istream& in)
	{
		std::getline(in, m_path, ')');

		switch(m_type)
		{
			case selector_type::Pipeline:
				reflection::parse_get_type(m_path, "VkGraphicsPipelineCreateInfo");
				break;
			case selector_type::Draw:
				reflection::parse_get_type(m_path, "VkCmdDrawIndexed");
				break;
			default:
				throw std::runtime_error("cannot work with selector type "+to_string(m_type));
		}
	}

	data_value vkstruct_data::get(selector_type stype, data_type type, VkHandle handle, local_context& ctx, rule& rule)
	{
		const void* structData;
		std::string structType;
		switch(stype)
		{
			case selector_type::Pipeline:
				structData = ctx.info->pipeline.info;
				structType = "VkGraphicsPipelineCreateInfo";
				break;
			case selector_type::Draw: {
				auto v = ctx.info->draw.info;
				if(std::holds_alternative<const reflection::VkCmdDrawIndexed*>(v))
				{
					structData = std::get<const reflection::VkCmdDrawIndexed*>(v);
					structType = "VkCmdDrawIndexed";
				}
				else if(std::holds_alternative<const reflection::VkCmdDraw*>(v))
				{
					structData = std::get<const reflection::VkCmdDraw*>(v);
					structType = "VkCmdDraw";
				}
				break;
			}
			default:
				throw std::runtime_error("cannot work with selector type "+to_string(stype));
		}

		switch(type)
		{
			case data_type::Number:
				return (double) std::any_cast<uint32_t>(reflection::parse_get(m_path, structData, structType));
			case data_type::String:
				return reflection::parse_get_string(m_path, structData, structType);
			case data_type::Raw: {
				uint32_t r = std::any_cast<uint32_t>(reflection::parse_get(m_path, structData, structType));
				std::vector<uint8_t> v(sizeof(r));
				std::copy((uint8_t*)&r, (uint8_t*)(&r+1), v.begin());
				return v; }
			default:
				throw std::runtime_error("cannot return data type "+to_string(type));
		}
	}

	bool vkstruct_data::supports(selector_type, data_type type)
	{
		return type == data_type::Number || type == data_type::String;
	}

	std::ostream& vkstruct_data::print(std::ostream& out)
	{
		out << "vkstruct(" << m_path << ")";
		return out;
	}

	void vkdescriptor_data::read(std::istream& in)
	{
		in >> m_set;
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		in >> m_binding;
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		in >> m_arrayIndex;
		skip_ws(in);
		check_stream(in, ')');
	}

	data_value vkdescriptor_data::get(selector_type, data_type, VkHandle, local_context& ctx, rule &)
	{
		VkDescriptorSet set = ctx.commandBufferState->descriptorSets.at(m_set);
		const DescriptorState& descriptorState = descriptorStates.at(set);
		const DescriptorBinding& binding = descriptorState.bindings.at(m_binding);
		const DescriptorElement& element = binding.arrayElements.at(m_arrayIndex);
		return element.handle;
	}

	bool vkdescriptor_data::supports(selector_type stype, data_type dtype)
	{
		return stype == Draw && dtype == Handle;
	}

	std::ostream& vkdescriptor_data::print(std::ostream& out)
	{
		out << "vkdescriptor(" << m_set << ", " << m_binding << ")";
		return out;
	}
}
