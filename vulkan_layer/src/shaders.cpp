#include "layer.hpp"
#include "objects.hpp"
#include "utils.hpp"

#include <fstream>
#include <filesystem>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>

#ifdef USE_SPIRV
#include "spirv_glsl.hpp"
#include "spirv.h"
#endif

#ifdef USE_GLSLANG
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#endif

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
using ShaderCacheEntry = std::vector<uint32_t>;
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

#include <iostream>

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
	if(!shader.parse(GetDefaultResources(), 100, false, messages))
	{
		return {false, std::string(shader.getInfoLog())};
	}

	glslang::TProgram program;
	program.addShader(&shader);
	if(!program.link(messages))
	{
		return {false, std::string(program.getInfoLog())};
	}

	program.buildReflection(EShReflectionIntermediateIO | EShReflectionSeparateBuffers |
		EShReflectionAllBlockVariables | EShReflectionUnwrapIOBlocks | EShReflectionAllIOVariables);

	glslang::GlslangToSpv(*program.getIntermediate(stage), shaderCode);
	glslang::FinalizeProcess();

	return {true, ""};
}
#endif

namespace CheekyLayer {

struct pre_shader_create_result {
	std::string hash_string;
	std::vector<uint32_t> temp_buffer;
};

pre_shader_create_result* device::pre_shader_create(VkShaderModuleCreateInfo *pCreateInfo) {
	VkShaderModuleCreateInfo& createInfo = *pCreateInfo; // we are allowed to modify *pCreateInfo
	pre_shader_create_result* res = new pre_shader_create_result{};

#ifdef USE_GLSLANG
	std::map<std::string, ShaderCacheEntry>::iterator it;
#endif

	std::string hash_string = sha256_string((uint8_t*) createInfo.pCode, createInfo.codeSize);
	if(inst->config.dump) {
		{
			std::string outputPath = inst->config.dump_directory / "shaders" / (hash_string+".spv");
			if(!std::filesystem::exists(outputPath))
			{
				std::ofstream out(outputPath, std::ios_base::binary);
				if(out.good())
					out.write((char*)createInfo.pCode, (size_t)createInfo.codeSize);
			}
		}
#ifdef USE_SPIRV
		try {
			std::vector<uint32_t> spirv_binary(createInfo.pCode, createInfo.pCode+(createInfo.codeSize/sizeof(uint32_t)));
			spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));

			spirv_cross::CompilerGLSL::Options options;
			options.version = 450;
			options.es = false;
			options.vulkan_semantics = true;
			glsl.set_common_options(options);

			uint32_t dummySampler = glsl.build_dummy_sampler_for_combined_images();
			if(dummySampler != 0)
			{
				// Set some defaults to make validation happy.
				glsl.set_decoration(dummySampler, spv::DecorationDescriptorSet, 0);
				glsl.set_decoration(dummySampler, spv::DecorationBinding, 0);
			}

			//glsl.build_combined_image_samplers();
			for (auto &remap : glsl.get_combined_image_samplers())
			{
				glsl.set_name(remap.combined_id, "SPIRV_Cross_Combined"+glsl.get_name(remap.image_id)+glsl.get_name(remap.sampler_id));
			}

			std::string code = glsl.compile();

			auto stages = glsl.get_entry_points_and_stages();
			std::string stage = "glsl";
			if(!stages.empty())
				stage = stage_to_string(stages[0].execution_model);

			std::ofstream out(inst->config.dump_directory / "shaders" / (hash_string+"."+stage));
			if(out.good())
				out << code;
		} catch(std::runtime_error& ex) {
			logger->error("Cannot decompile: {}", ex.what());
		}
#endif
	}

	if(inst->config.override) {
		if(has_override(hash_string)) {
			std::ifstream in(inst->config.override_directory / "shaders" / (hash_string+".spv"));
			if(in.good()) {
				in.seekg(0, std::ios::end);
				size_t length = in.tellg();
				in.seekg(0, std::ios::beg);

				if(length % sizeof(uint32_t) != 0) {
					logger->error("Shader override size is not a multiple of 4! size={}", length);
					throw std::runtime_error("Shader override size is not a multiple of 4!");
				}
				length /= sizeof(uint32_t);

				logger->info("Found shader override! size={}", length);

				res->temp_buffer.resize(length);
				in.read((char*) res->temp_buffer.data(), length*sizeof(uint32_t));

				createInfo.pCode = res->temp_buffer.data();
				createInfo.codeSize = length;
			}
#ifdef USE_GLSLANG
			else if((it = shaderCache.find(hash_string)) != shaderCache.end()) {
				createInfo.pCode = it->second.data();
				createInfo.codeSize = it->second.size();
				logger->info("Found cached shader override! size={}", it->second.size());
			}
			else {
				EShLanguage stage;
				std::string code;

				for(std::string suffix : {"vert", "frag", "comp"}) {
					std::ifstream in(inst->config.override_directory / "shaders" / (hash_string + "." + suffix));
					if(!in.good())
						continue;

					stage = string_to_stage(suffix);
					code = std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
					break;
				}

				if(!code.empty()) {
					std::vector<uint32_t> spv;
					auto [result, message] = compileShader(stage, code, spv);

					if(result) {
						size_t length = spv.size() * sizeof(uint32_t);
						logger->info("Found and compiled shader override! size={}", length);

						res->temp_buffer = spv;

						createInfo.pCode = res->temp_buffer.data();
						createInfo.codeSize = length;

						shaderCache[hash_string] = spv;
					} else {
						logger->error("Compilation failed: {}", message);
					}
				}
			}
#endif
		}
	}

	res->hash_string = std::move(hash_string);
	return res;
}

void device::post_shader_create(const VkShaderModuleCreateInfo* pCreateInfo, rules::VkHandle handle, pre_shader_create_result* res) {
	spv_reflect::ShaderModule reflection(pCreateInfo->codeSize, pCreateInfo->pCode);
	if(reflection.GetResult() == SPV_REFLECT_RESULT_SUCCESS) {
		shaderReflections[handle] = std::move(reflection);
	} else {
		logger->error("shader reflection failed: {}!", fmt::underlying(reflection.GetResult()));
	}

	put_hash(handle, res->hash_string);
	rules::calling_context ctx{};
	execute_rules(rules::selector_type::Shader, handle, ctx);

	delete res;
}

VkResult device::CreateShaderModule(const VkShaderModuleCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule)
{
	VkShaderModuleCreateInfo createInfo = *pCreateInfo;
	auto res = pre_shader_create(&createInfo);

	VkResult result = dispatch.CreateShaderModule(handle, &createInfo, pAllocator, pShaderModule);
	if(result != VK_SUCCESS)
		return result;

	rules::VkHandle handle = (rules::VkHandle)*pShaderModule;
	rules::VkHandle customHandle = customShaderHandles[handle] = (rules::VkHandle) currentCustomShaderHandle++;

	if(has_debug) {
		std::string name = "Shader "+res->hash_string;

		VkDebugUtilsObjectNameInfoEXT info{};
		info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		info.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
		info.objectHandle = (uint64_t) *pShaderModule;
		info.pObjectName = name.c_str();
		dispatch.SetDebugUtilsObjectNameEXT(this->handle, &info);
	}

	post_shader_create(pCreateInfo, customHandle, res);

	return result;
}

}

VK_LAYER_EXPORT VkResult VKAPI_CALL CheekyLayer_CreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo, VkAllocationCallbacks *pAllocator,
		VkShaderModule *pShaderModule)
{
	return CheekyLayer::get_device(device).CreateShaderModule(pCreateInfo, pAllocator, pShaderModule);
}
