#include <vulkan/vulkan_raii.hpp>
#include <iostream>
#include <iomanip>
#include <span>
#include <ranges>
#include <fstream>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Include/Types.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

uint32_t findMemoryType(vk::PhysicalDeviceMemoryProperties const& memoryProperties, int32_t typeBits, vk::MemoryPropertyFlags requirementsMask)
{
    uint32_t typeIndex = uint32_t( ~0 );
    for ( uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++ )
    {
        if((typeBits & 1) && ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask))
        {
            typeIndex = i;
            break;
        }
        typeBits >>= 1;
    }
    assert( typeIndex != uint32_t( ~0 ) );
    return typeIndex;
}

std::string read_file(std::string path)
{
    std::ifstream in{path};
    if(!in) throw std::runtime_error("file not found");
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::tuple<std::unique_ptr<glslang::TShader>, std::unique_ptr<glslang::TProgram>> compile_shader(std::string shaderCode, EShLanguage stage)
{
    const char* shaderStrings[1];
    shaderStrings[0] = shaderCode.data();
    std::unique_ptr<glslang::TShader> shader = std::make_unique<glslang::TShader>(stage);
    shader->setStrings(shaderStrings, 1);
    shader->setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
    shader->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetClientVersion::EShTargetVulkan_1_1);
    shader->setEnvTarget(glslang::EShTargetLanguage::EShTargetSpv, glslang::EShTargetLanguageVersion::EShTargetSpv_1_3);
    shader->setEntryPoint("main");
    shader->setSourceEntryPoint("main");

    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
    if(!shader->parse(GetDefaultResources(), 100, false, messages))
    {
        throw std::runtime_error(shader->getInfoLog());
    }
    
    std::unique_ptr<glslang::TProgram> program = std::make_unique<glslang::TProgram>();
	program->addShader(shader.get());
	if(!program->link(messages))
	{
        throw std::runtime_error(program->getInfoLog());
	}
    return {std::move(shader), std::move(program)};
}


int main(int argc, char** argv)
{
    constexpr uint32_t width = 4096;
    constexpr uint32_t height = 4096;

    std::string vertexDataFile = argv[1];
    std::string shaderFile = argv[2];
    std::string shaderCode = read_file(shaderFile);

    glslang::InitializeProcess();

    auto [_fragmentShader, fragmentProgram] = compile_shader(shaderCode, EShLanguage::EShLangFragment);
    std::vector<unsigned int> fragmentSpv{};
    glslang::GlslangToSpv(*fragmentProgram->getIntermediate(EShLanguage::EShLangFragment), fragmentSpv);

	fragmentProgram->buildReflection(EShReflectionIntermediateIO | EShReflectionSeparateBuffers | 
		EShReflectionAllBlockVariables | EShReflectionUnwrapIOBlocks | EShReflectionAllIOVariables);
    std::size_t framebufferCount = fragmentProgram->getNumPipeOutputs();
    std::size_t inputCount = fragmentProgram->getNumPipeInputs();

    std::vector<vk::VertexInputAttributeDescription> vertexInputAttributes;
    std::ostringstream vertexCode{};
    vertexCode << "#version 450\n";
    vertexCode << "layout(location = 0) in vec4 position;\n";
    vertexInputAttributes.push_back(vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32A32Sfloat, 0});
    for(int i=0; i<inputCount; i++)
    {
        const auto& input = fragmentProgram->getPipeInput(i);
        const auto* type = input.getType();
        uint32_t location = type->getQualifier().layoutLocation;
        uint32_t inputLocation = location + 1;
        int components = type->computeNumComponents();
        std::string typeString = components == 1 ? "float" : "vec"+std::to_string(components);
        vertexCode << "layout(location = " << inputLocation << ") in " << typeString << " input" << i << ";\n";
        vertexCode << "layout(location = " << location << ") out " << typeString << " output" << i << ";\n";

        vk::Format format = std::array<vk::Format, 4>{
            vk::Format::eR32Sfloat,
            vk::Format::eR32G32Sfloat,
            vk::Format::eR32G32B32Sfloat,
            vk::Format::eR32G32B32A32Sfloat
        }[components];

        vertexInputAttributes.push_back(vk::VertexInputAttributeDescription{inputLocation, 0, format, static_cast<uint32_t>(inputLocation*4*sizeof(float))});
    }
    vertexCode << "void main() {\n";
    vertexCode << "    gl_Position = position;\n";
    for(int i=0; i<inputCount; i++)
    {
        vertexCode << "    output" << i << " = input" << i << ";\n"; 
    }
    vertexCode << "}\n";
    uint32_t totalInputStride = static_cast<uint32_t>(vertexInputAttributes.size()*4*sizeof(float));

    auto [_vertexShader, vertexProgram] = compile_shader(vertexCode.str(), EShLanguage::EShLangVertex);
    std::vector<unsigned int> vertexSpv{};
    glslang::GlslangToSpv(*vertexProgram->getIntermediate(EShLanguage::EShLangVertex), vertexSpv);

    vk::raii::Context context;
    
    vk::ApplicationInfo appInfo{"texture_renderer", 1, "texture_renderer", 1, VK_API_VERSION_1_1};

    vk::raii::Instance instance{context, vk::InstanceCreateInfo{{}, &appInfo}};

    vk::raii::PhysicalDevices physicalDevices{instance};
    vk::raii::PhysicalDevice physicalDevice = physicalDevices.at(0);
    for(const auto& device : physicalDevices)
    {
        const auto& props = device.getProperties();
        std::cout << "Device #" << props.deviceID << ": " << std::quoted(std::string(props.deviceName)) << " of type " << vk::to_string(props.deviceType) << '\n';
        if(props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu || props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
            physicalDevice = device;
    }
    std::cout << '\n';
    
    const auto& props = physicalDevice.getProperties();
    std::cout << "Picked device #" << props.deviceID << ": " << std::quoted(std::string(props.deviceName)) << " of type " << vk::to_string(props.deviceType) << ".\n";
    const auto& memoryProperties = physicalDevice.getMemoryProperties();

    auto queues = physicalDevice.getQueueFamilyProperties();
    uint32_t queueFamilyIndex = std::distance(queues.begin(), std::find_if(queues.begin(), queues.end(), [](const vk::QueueFamilyProperties q){
        return (q.queueFlags & vk::QueueFlagBits::eGraphics) && (q.queueFlags & vk::QueueFlagBits::eTransfer);
    }));
    std::cout << "Using queue family " << queueFamilyIndex << ".\n";

    float priority = 1.0f;
    std::array<vk::DeviceQueueCreateInfo, 1> queueCreateInfos = {
        vk::DeviceQueueCreateInfo{{}, queueFamilyIndex, 1, &priority}
    };
    vk::raii::Device device(physicalDevice, vk::DeviceCreateInfo{{}, queueCreateInfos});
    vk::raii::Queue queue{device, queueFamilyIndex, 0};
    
    vk::raii::CommandPool pool{device, vk::CommandPoolCreateInfo{{}, queueFamilyIndex}};
    vk::raii::CommandBuffers buffers{device, vk::CommandBufferAllocateInfo{*pool, vk::CommandBufferLevel::ePrimary, 1}};

    std::vector<vk::raii::Image> framebufferImages;
    std::vector<vk::raii::DeviceMemory> framebufferMemories;
    std::vector<vk::raii::ImageView> framebufferImageViews;

    for(int i=0; i<framebufferCount; i++)
    {
        vk::raii::Image image{device, vk::ImageCreateInfo{
            {}, vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb, vk::Extent3D{width, height, 1},
            1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
            vk::SharingMode::eExclusive
        }};
        auto memoryRequirements = image.getMemoryRequirements();
        vk::raii::DeviceMemory memory{device, vk::MemoryAllocateInfo{
            memoryRequirements.size,
            findMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        }};
        image.bindMemory(*memory, 0);

        vk::raii::ImageView imageView{device, vk::ImageViewCreateInfo{
            {}, *image, vk::ImageViewType::e2D, vk::Format::eR8G8B8A8Srgb,
            vk::ComponentMapping{},
            vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
        }};

        framebufferImages.push_back(std::move(image));
        framebufferMemories.push_back(std::move(memory));
        framebufferImageViews.push_back(std::move(imageView));
    }
    vk::raii::ShaderModule vertexShader{device, vk::ShaderModuleCreateInfo{
        {}, vertexSpv.size()*sizeof(uint32_t), vertexSpv.data()
    }};
    vk::raii::ShaderModule fragmentShader{device, vk::ShaderModuleCreateInfo{
        {}, fragmentSpv.size()*sizeof(uint32_t), fragmentSpv.data()
    }};
    std::array<vk::PipelineShaderStageCreateInfo, 2> pipelineStages{
        vk::PipelineShaderStageCreateInfo{
            {}, vk::ShaderStageFlagBits::eVertex, *vertexShader, "main"
        },
        vk::PipelineShaderStageCreateInfo{
            {}, vk::ShaderStageFlagBits::eFragment, *fragmentShader, "main"
        }
    };
    vk::VertexInputBindingDescription pipelineInputBinding{0, totalInputStride, vk::VertexInputRate::eVertex};
    vk::PipelineVertexInputStateCreateInfo pipelineInputState{{}, pipelineInputBinding, vertexInputAttributes};
    vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyState{{}, vk::PrimitiveTopology::eTriangleList, false};
    vk::PipelineTessellationStateCreateInfo pipelineTessellationState{};
    vk::PipelineViewportStateCreateInfo pipelineViewportState{}; // TODO
    vk::PipelineRasterizationStateCreateInfo pipelineRasterizationState{
        {}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone,
        vk::FrontFace::eCounterClockwise, VK_FALSE, {}, {}, {}, 1.0
    };
    vk::PipelineMultisampleStateCreateInfo pipelineMultisampleState{{}, vk::SampleCountFlagBits::e1, false};
    vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilState{{}, false};
    vk::PipelineColorBlendStateCreateInfo pipelineColorBlendState{}; // TODO
    vk::PipelineDynamicStateCreateInfo pipelineDynamicState{};
    vk::GraphicsPipelineCreateInfo pipelineCreateInfo{
        {}, pipelineStages, 
        &pipelineInputState, &pipelineInputAssemblyState, &pipelineTessellationState,
        &pipelineViewportState, &pipelineRasterizationState, &pipelineMultisampleState,
        &pipelineDepthStencilState, &pipelineColorBlendState, &pipelineDynamicState
    };
    vk::raii::Pipeline graphicsPipeline{device, std::nullptr_t(), pipelineCreateInfo};
    
    vk::raii::CommandBuffer commandBuffer{std::move(buffers[0])};
    commandBuffer.begin(vk::CommandBufferBeginInfo{});
    commandBuffer.end();

    vk::raii::Fence fence{device, vk::FenceCreateInfo{}};
    queue.submit(vk::SubmitInfo{{}, {}, *commandBuffer}, *fence);

    device.waitForFences(*fence, true, UINT64_MAX);
}
