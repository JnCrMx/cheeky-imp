#include "rules/data.hpp"
#include "rules/execution_env.hpp"
#include "rules/rules.hpp"
#include "utils.hpp"

namespace CheekyLayer::rules::datas
{
	data_register<unpack_data> unpack_data::reg("unpack");
	data_register<pack_data> pack_data::reg("pack");

	raw_type from_string(std::string s)
	{
		if(s=="SInt8") return raw_type::SInt8;
		if(s=="SInt16") return raw_type::SInt16;
		if(s=="SInt32") return raw_type::SInt32;
		if(s=="SInt64") return raw_type::SInt64;

		if(s=="UInt8") return raw_type::UInt8;
		if(s=="UInt16") return raw_type::UInt16;
		if(s=="UInt32") return raw_type::UInt32;
		if(s=="UInt64") return raw_type::UInt64;

		if(s=="Float") return raw_type::Float;
		if(s=="Double") return raw_type::Double;

		if(s=="Array") return raw_type::Array;

		throw RULE_ERROR("unsupported raw_type: "+s);
	}

	std::string to_string(raw_type t)
	{
		switch(t)
		{
			case raw_type::SInt8: return "SInt8";
			case raw_type::SInt16: return "SInt16";
			case raw_type::SInt32: return "SInt32";
			case raw_type::SInt64: return "SInt64";

			case raw_type::UInt8: return "UInt8";
			case raw_type::UInt16: return "UInt16";
			case raw_type::UInt32: return "UInt32";
			case raw_type::UInt64: return "UInt64";

			case raw_type::Float: return "Float";
			case raw_type::Double: return "Double";

			case raw_type::Array: return "Array";
		}
	}

	void unpack_data::read(std::istream& in)
	{
		std::string type;
		std::getline(in, type, ',');
		m_rawType = from_string(type);
		skip_ws(in);

		bool array = m_rawType == raw_type::Array;
		if(array)
		{
			std::string type;
			std::getline(in, type, ',');
			m_rawType = from_string(type);
			skip_ws(in);

			in >> m_count;
			skip_ws(in);
			check_stream(in, ',');
			skip_ws(in);
		}
		else
		{
			m_count = -1;
		}

		in >> m_offset;
		skip_ws(in);
		check_stream(in, ',');
		skip_ws(in);

		m_src = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_src->supports(m_type, data_type::Raw))
		{
			std::ostringstream of;
			of << "Source \"";
			m_src->print(of);
			of << "\" does not support type Raw.";
			throw RULE_ERROR(of.str());
		}
	}

	static double unpack(raw_type type, void* ptr)
	{
		switch(type)
		{
			case raw_type::SInt8: return (double) (*((int8_t*)ptr));
			case raw_type::SInt16: return (double) (*((int16_t*)ptr));
			case raw_type::SInt32: return (double) (*((int32_t*)ptr));
			case raw_type::SInt64: return (double) (*((int64_t*)ptr));

			case raw_type::UInt8: return (double) (*((uint8_t*)ptr));
			case raw_type::UInt16: return (double) (*((uint16_t*)ptr));
			case raw_type::UInt32: return (double) (*((uint32_t*)ptr));
			case raw_type::UInt64: return (double) (*((uint64_t*)ptr));

			case raw_type::Float: return (double) (*((float*)ptr));
			case raw_type::Double: return (double) (*((double*)ptr));

			case raw_type::Array: __builtin_unreachable(); break;
		}
	}

	data_value unpack_data::get(selector_type type, data_type dtype, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(!supports(type, dtype))
			throw RULE_ERROR("cannot return data type "+CheekyLayer::rules::to_string(dtype));

		std::vector<uint8_t> v = std::get<std::vector<uint8_t>>(m_src->get(type, data_type::Raw, handle, ctx, rule));

		int count = m_count == -1 ? 1 : m_count;
		size_t theSize;
		switch(m_rawType)
		{
			case raw_type::SInt8: theSize = sizeof(int8_t); break;
			case raw_type::SInt16: theSize = sizeof(int16_t); break;
			case raw_type::SInt32: theSize = sizeof(int32_t); break;
			case raw_type::SInt64: theSize = sizeof(int64_t); break;

			case raw_type::UInt8: theSize = sizeof(uint8_t); break;
			case raw_type::UInt16: theSize = sizeof(uint16_t); break;
			case raw_type::UInt32: theSize = sizeof(uint32_t); break;
			case raw_type::UInt64: theSize = sizeof(uint64_t); break;

			case raw_type::Float: theSize = sizeof(float); break;
			case raw_type::Double: theSize = sizeof(double); break;

			case raw_type::Array: __builtin_unreachable(); break;
		}
		if(v.size()-m_offset < theSize*count)
			throw RULE_ERROR("Raw data of size "+std::to_string(v.size())+" is not big enough to contain "+std::to_string(count)+" number(s) of type "
				+to_string(m_rawType)+" at offset "+std::to_string(m_offset));

		void* ptr = v.data() + m_offset;
		if(m_count == -1)
		{
			return unpack(m_rawType, ptr);
		}
		else
		{
			data_list list{};
			list.values.resize(m_count);
			for(int i=0; i<m_count; i++)
			{
				list.values[i] = unpack(m_rawType, (char*)ptr + theSize*i);
			}
			return list;
		}
	}

	bool unpack_data::supports(selector_type, data_type type)
	{
		return m_count == -1 ? type == Number : type == List;
	}

	std::ostream& unpack_data::print(std::ostream& out)
	{
		if(m_count == -1)
		{
			out << "unpack(" << to_string(m_rawType) << ", " << m_offset << ", ";
			m_src->print(out);
			out << ")";
		}
		else
		{
			out << "unpack(" << to_string(raw_type::Array) << ", " << to_string(m_rawType) << ", " << m_count << ", " << m_offset << ", ";
			m_src->print(out);
			out << ")";
		}
		return out;
	}

	void pack_data::read(std::istream& in)
	{
		std::string type;
		std::getline(in, type, ',');
		m_rawType = from_string(type);
		skip_ws(in);

		if(m_rawType == raw_type::Array)
			throw RULE_ERROR("\""+to_string(m_rawType)+"\" is not supported");

		m_src = read_data(in, m_type);
		check_stream(in, ')');

		if(!m_src->supports(m_type, data_type::Number))
		{
			std::ostringstream of;
			of << "Source \"";
			m_src->print(of);
			of << "\" does not support type Number.";
			throw RULE_ERROR(of.str());
		}
	}

	data_value pack_data::get(selector_type type, data_type dtype, VkHandle handle, local_context& ctx, rule& rule)
	{
		if(dtype != data_type::Raw)
			throw RULE_ERROR("cannot return data type "+CheekyLayer::rules::to_string(dtype));

		double d = std::get<double>(m_src->get(type, data_type::Number, handle, ctx, rule));

		size_t minSize;
		switch(m_rawType)
		{
			case raw_type::SInt8: minSize = sizeof(int8_t); break;
			case raw_type::SInt16: minSize = sizeof(int16_t); break;
			case raw_type::SInt32: minSize = sizeof(int32_t); break;
			case raw_type::SInt64: minSize = sizeof(int64_t); break;

			case raw_type::UInt8: minSize = sizeof(uint8_t); break;
			case raw_type::UInt16: minSize = sizeof(uint16_t); break;
			case raw_type::UInt32: minSize = sizeof(uint32_t); break;
			case raw_type::UInt64: minSize = sizeof(uint64_t); break;

			case raw_type::Float: minSize = sizeof(float); break;
			case raw_type::Double: minSize = sizeof(double); break;

			case raw_type::Array: __builtin_unreachable(); break;
		}

		std::vector<uint8_t> v(minSize);
		void* ptr = v.data();

		switch(m_rawType)
		{
			case raw_type::SInt8: (*(int8_t*)ptr) = (int8_t) d; break;
			case raw_type::SInt16: (*(int16_t*)ptr) = (int16_t) d; break;
			case raw_type::SInt32: (*(int32_t*)ptr) = (int32_t) d; break;
			case raw_type::SInt64: (*(int64_t*)ptr) = (int64_t) d; break;

			case raw_type::UInt8: (*(uint8_t*)ptr) = (uint8_t) d; break;
			case raw_type::UInt16: (*(uint16_t*)ptr) = (uint16_t) d; break;
			case raw_type::UInt32: (*(uint32_t*)ptr) = (uint32_t) d; break;
			case raw_type::UInt64: (*(uint64_t*)ptr) = (uint64_t) d; break;

			case raw_type::Float: (*(float*)ptr) = (float) d; break;
			case raw_type::Double: (*(double*)ptr) = (double) d; break;

			case raw_type::Array: __builtin_unreachable(); break;
		}
		return v;
	}

	bool pack_data::supports(selector_type, data_type type)
	{
		return type == Raw;
	}

	std::ostream& pack_data::print(std::ostream& out)
	{
		out << "pack(" << to_string(m_rawType) << ", ";
		m_src->print(out);
		out << ")";
		return out;
	}
}
