#include "rules/data.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"

namespace CheekyLayer::rules::datas
{
	data_register<convert_data> convert_data::reg("convert");

	void convert_data::read(std::istream& in)
	{
		std::string st;
		std::getline(in, st, ',');
		skip_ws(in);
		srcType = data_type_from_string(st);

		std::string dt;
		std::getline(in, dt, ',');
		skip_ws(in);
		dstType = data_type_from_string(dt);

		m_src = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_src->supports(m_type, srcType))
			throw RULE_ERROR("data does not support source data type "+to_string(srcType));
	}

	data_value convert_data::get(selector_type stype, data_type type, VkHandle handle, global_context& global, local_context& local, rule& rule)
	{
		if(type != dstType)
			throw RULE_ERROR("cannot return data type "+to_string(type));

		data_value value = m_src->get(stype, srcType, handle, global, local, rule);
		if(srcType == dstType)
			return value;

		switch(srcType)
		{
			case data_type::String: {
				std::string str = std::get<std::string>(value);
				switch(dstType)
				{
					case data_type::Raw:
						return std::vector<uint8_t>(str.begin(), str.end());
					case data_type::Number:
						return std::stod(str);
					default:
						throw RULE_ERROR("no known conversion from "+to_string(srcType)+" to "+to_string(dstType));
				}
			}
			case data_type::Raw: {
				std::vector<uint8_t> raw = std::get<std::vector<uint8_t>>(value);
				switch(dstType)
				{
					case data_type::String:
						return std::string(raw.begin(), raw.end());
					default:
						throw RULE_ERROR("no known conversion from "+to_string(srcType)+" to "+to_string(dstType));
				}
			}
			case data_type::Number: {
				double d = std::get<double>(value);
				switch(dstType)
				{
					case data_type::String:
						return std::to_string(d);
					default:
						throw RULE_ERROR("no known conversion from "+to_string(srcType)+" to "+to_string(dstType));
				}
			}
			default:
				throw RULE_ERROR("no known conversion from "+to_string(srcType)+" to "+to_string(dstType));
		}
	}

	bool convert_data::supports(selector_type, data_type type)
	{
		return type == dstType;
	}

	std::ostream& convert_data::print(std::ostream& out)
	{
		out << "convert(" << to_string(srcType) << ", " << to_string(dstType) << ", ";
		m_src->print(out);
		out << ")";
		return out;
	}
}
