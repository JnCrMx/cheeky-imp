#include "layer.hpp"
#include "dispatch.hpp"
#include "utils.hpp"
#include "shaders.hpp"
#include <fstream>
#include <filesystem>
#include <iterator>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>

#ifdef USE_SPIRV
#include "spirv_glsl.hpp"
#include "spirv.h"
#endif

#ifdef USE_GLSLANG
#include <ShaderLang.h>
#include <ResourceLimits.h>
#include <GlslangToSpv.h>
#endif

#include <streambuf>
#include <cstdint>
#include <stdexcept>

using CheekyLayer::logger;
using CheekyLayer::rules::VkHandle;

static uint64_t currentCustonShaderHandle = 0xABC1230000;
std::map<VkHandle, VkHandle> customShaderHandles;

#ifdef USE_SPIRV
std::string stage_to_string(spv::ExecutionModel stage)
{
	switch(stage)
	{
		case spv::ExecutionModel::ExecutionModelVertex:
			return "vert";
		case spv::ExecutionModel::ExecutionModelFragment:
			return "frag";
		default:
			return "glsl";
	}
}
#endif

#ifdef USE_GLSLANG
struct ShaderCacheEntry
{
	uint32_t* pointer;
	size_t size;
};
std::map<std::string, ShaderCacheEntry> shaderCache;

EShLanguage string_to_stage(std::string string)
{
	if(string == "vert")
		return EShLanguage::EShLangVertex;
	if(string == "frag")
		return EShLanguage::EShLangFragment;
	if(string == "comp")
		return EShLanguage::EShLangCompute;
	throw std::runtime_error("unknown stage: "+string);
}

std::tuple<bool, std::string> compileShader(EShLanguage stage, std::string glslCode, std::vector<unsigned int>& shaderCode)
{
	const char * shaderStrings[1];
	shaderStrings[0] = glslCode.data();

	glslang::InitializeProcess();
	glslang::TShader shader(stage);
	shader.setStrings(shaderStrings, 1);
	shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetClientVersion::EShTargetVulkan_1_1);
	shader.setEnvTarget(glslang::EShTargetLanguage::EShTargetSpv, glslang::EShTargetLanguageVersion::EShTargetSpv_1_3);
	shader.setEntryPoint("main");
	shader.setSourceEntryPoint("main");

	EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
	if(!shader.parse(&glslang::DefaultTBuiltInResource, 100, false, messages))
	{
		return {false, std::string(shader.getInfoLog())};
	}

	glslang::TProgram program;
	program.addShader(&shader);
	if(!program.link(messages))
	{
		return {false, std::string(program.getInfoLog())};
	}
	glslang::GlslangToSpv(*program.getIntermediate(stage), shaderCode);
	glslang::FinalizeProcess();

	return {true, ""};
}
#endif

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateShaderModule(VkDevice device, VkShaderModuleCreateInfo *pCreateInfo, VkAllocationCallbacks *pAllocator,
		VkShaderModule *pShaderModule)
{
	//scoped_lock l(global_lock);

#ifdef USE_GLSLANG
	std::map<std::string, ShaderCacheEntry>::iterator it;
#endif

	char hash[65];
	sha256_string((uint8_t*) pCreateInfo->pCode, pCreateInfo->codeSize, hash);
	std::string hash_string(hash);

	CheekyLayer::active_logger log = *logger << logger::begin << "CreateShaderModule: " << "size=" << pCreateInfo->codeSize << " hash=" << hash_string;

	if(global_config.map<bool>("dump", CheekyLayer::config::to_bool))
	{
		{
			std::string outputPath = global_config["dumpDirectory"]+"/shaders/"+hash_string+".spv";
			if(!std::filesystem::exists(outputPath))
			{
				std::ofstream out(outputPath, std::ios_base::binary);
				if(out.good())
					out.write((char*)pCreateInfo->pCode, (size_t)pCreateInfo->codeSize);
			}
		}

#ifdef USE_SPIRV
		try
		{
			std::vector<uint32_t> spirv_binary(pCreateInfo->pCode, pCreateInfo->pCode+(pCreateInfo->codeSize/sizeof(uint32_t)));
			spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));

			spirv_cross::CompilerGLSL::Options options;
			options.version = 450;
			options.es = false;
			options.vulkan_semantics = true;
			glsl.set_common_options(options);

			glsl.build_combined_image_samplers();
			for (auto &remap : glsl.get_combined_image_samplers())
			{
				glsl.set_name(remap.combined_id, "SPIRV_Cross_Combined"+glsl.get_name(remap.image_id)+glsl.get_name(remap.sampler_id));
			}

			uint32_t dummySampler = glsl.build_dummy_sampler_for_combined_images();
			if(dummySampler != 0)
			{
				// Set some defaults to make validation happy.
				glsl.set_decoration(dummySampler, spv::DecorationDescriptorSet, 0);
				glsl.set_decoration(dummySampler, spv::DecorationBinding, 0);
			}

			std::string code = glsl.compile();

			auto stages = glsl.get_entry_points_and_stages();
			std::string stage = "glsl";
			if(!stages.empty())
				stage = stage_to_string(stages[0].execution_model);

			std::ofstream out2(global_config["dumpDirectory"]+"/shaders/"+hash_string+"."+stage);
			if(out2.good())
				out2 << code;
		}
		catch(std::runtime_error& ex)
		{
			log << logger::error << "Cannot decompile: " << ex.what();
		}
#endif
	}
	if(global_config.map<bool>("override", CheekyLayer::config::to_bool) && has_override(hash_string))
	{
		std::ifstream in(global_config["overrideDirectory"]+"/shaders/"+hash_string+".spv");
		if(in.good())
		{
			in.seekg(0, std::ios::end);
			size_t length = in.tellg();
			in.seekg(0, std::ios::beg);

			log << " found shader override! " << "size=" << length;

			uint8_t *buffer = (uint8_t*) malloc(length);
			in.read((char*) buffer, length);

			pCreateInfo->pCode = (uint32_t*) buffer;
			pCreateInfo->codeSize = length;
		}
#ifdef USE_GLSLANG
		else if((it = shaderCache.find(hash_string)) != shaderCache.end())
		{
			pCreateInfo->pCode = it->second.pointer;
			pCreateInfo->codeSize = it->second.size;
			log << " found cached shader override! " << "size=" << it->second.size;
		}
		else
		{
			EShLanguage stage;
			std::string code;

			for(std::string suffix : {"vert", "frag", "comp"})
			{
				std::ifstream in2(global_config["overrideDirectory"]+"/shaders/"+hash_string+"."+suffix);
				if(in2.good())
				{
					stage = string_to_stage(suffix);

					in2.seekg(0, std::ios::end);
					code.reserve(in2.tellg());
					in2.seekg(0, std::ios::beg);
					code.assign(std::istreambuf_iterator<char>(in2), std::istreambuf_iterator<char>());

					break;
				}
			}

			if(!code.empty())
			{
				std::vector<uint32_t> spv;
				auto [result, message] = compileShader(stage, code, spv);

				if(result)
				{
					size_t length = spv.size() * sizeof(uint32_t);
					log << " found and compiled shader override! " << "size=" << length;

					uint32_t *buffer = (uint32_t*) malloc(length);
					std::copy(spv.begin(), spv.end(), buffer);
					
					pCreateInfo->pCode = buffer;
					pCreateInfo->codeSize = length;

					shaderCache[hash_string] = {buffer, length};
				}
				else
				{
					log << logger::error << "compilation failed: " << message;
				}
			}
		}
#endif
	}

	VkResult result = device_dispatch[GetKey(device)].CreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);

	if(result == VK_SUCCESS)
	{
		VkHandle handle = (VkHandle)*pShaderModule;
		VkHandle customHandle = customShaderHandles[handle] = (VkHandle) currentCustonShaderHandle++;

		/*auto p = CheekyLayer::rule_env.hashes.find(handle);
		if(p != CheekyLayer::rule_env.hashes.end() && hash_string != p->second)
		{
			log << " Shader module collision: " << handle << " (formerly known as " << p->second << ") will be replaced";
		}*/

		CheekyLayer::rules::rule_env.hashes[customHandle] = hash_string;
		CheekyLayer::rules::local_context ctx = {log};
		CheekyLayer::rules::execute_rules(rules, CheekyLayer::rules::selector_type::Shader, customHandle, ctx);
	}

	log << logger::end;
	return result;
}
