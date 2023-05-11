#include <vulkan/vulkan_raii.hpp>
#include <iostream>
#include <iomanip>
#include <span>
#include <ranges>
#include <fstream>
#include <filesystem>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Include/Types.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <cxxopts.hpp>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "renderdoc_app.h"

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
    cxxopts::Options options("texture_renderer", "Create trivial UVs and create a texture based on a fragment shader");
    options.add_options()
        ("w,width", "Width of the texture to generate", cxxopts::value<uint32_t>()->default_value("4096"), "number")
        ("h,height", "Height of the texture to generate", cxxopts::value<uint32_t>()->default_value("4096"), "number")
        ("o,output", "File to write the generated texture to", cxxopts::value<std::filesystem::path>()->default_value("texture.png"), "path")
        ("attachment", "Index of color attachment to output", cxxopts::value<uint32_t>()->default_value("0"), "number")
        ("shader", "File containing fragment shader to use for generating texture", cxxopts::value<std::filesystem::path>(), "path")
        ("vertices", "CSV file containing the output of a vertex shader (e.g. export from RenderDoc)", cxxopts::value<std::filesystem::path>(), "path")
        ("pipeline", "JSON file describing the pipeline layout and used uniforms", cxxopts::value<std::filesystem::path>(), "path")
        ("help", "Print this help message")
        ("v,verbose", "Verbose output")
        ("debug-renderdoc", "Create RenderDoc capture", cxxopts::value<std::filesystem::path>()->implicit_value("/usr/lib/librenderdoc.so"), "path to librenderdoc.so")
        ;
    cxxopts::ParseResult optionResult;
    try
    {
        optionResult = options.parse(argc, argv);
    }
    catch(const cxxopts::exceptions::parsing& ex)
    {
        std::cerr << ex.what() << "\n\n" << options.help() << std::endl;
        return 2;
    }

    if(optionResult.count("help"))
    {
        std::cout << options.help() << std::endl;
        return 0;
    }
    if(!optionResult.count("shader") || !optionResult.count("vertices") || !optionResult.count("pipeline"))
    {
        std::cerr << options.help() << std::endl;
        return 2;
    }
    bool verbose = optionResult.count("verbose");

    RENDERDOC_API_1_1_2 *rdoc_api = NULL;
    if(optionResult.count("debug-renderdoc"))
    {
        auto path = optionResult["debug-renderdoc"].as<std::filesystem::path>();
        if(void *mod = dlopen(path.c_str(), RTLD_NOW))
        {
            pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
            int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api);
            assert(ret == 1);
        }
        else
        {
            std::cerr << "Failed to load RenderDoc: " << dlerror() << '\n';
        }
    }

    uint32_t width = optionResult["width"].as<uint32_t>();
    uint32_t height = optionResult["height"].as<uint32_t>();
    uint32_t framebufferIndex = optionResult["attachment"].as<uint32_t>();

    auto vertexDataFile = optionResult["vertices"].as<std::filesystem::path>();
    auto shaderFile = optionResult["shader"].as<std::filesystem::path>();
    auto pipelineDescriptionFile = optionResult["pipeline"].as<std::filesystem::path>();
    std::string shaderCode = read_file(shaderFile);
    nlohmann::json pipelineDescription;
    {
        std::ifstream in{pipelineDescriptionFile};
        in >> pipelineDescription;
    }

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
        }[components-1];

        vertexInputAttributes.push_back(vk::VertexInputAttributeDescription{inputLocation, 0, format, static_cast<uint32_t>(inputLocation*4*sizeof(float))});
    }
    vertexCode << "void main() {\n";
    vertexCode << "    gl_Position = position;\n";
    for(int i=0; i<inputCount; i++)
    {
        vertexCode << "    output" << i << " = input" << i << ";\n"; 
    }
    vertexCode << "}\n";
    uint32_t totalInputStride = static_cast<uint32_t>(vertexInputAttributes.size()*sizeof(glm::vec4));

    if(verbose) std::cout << "Using generated vertex shader:\n" << vertexCode.str();

    auto [_vertexShader, vertexProgram] = compile_shader(vertexCode.str(), EShLanguage::EShLangVertex);
    std::vector<unsigned int> vertexSpv{};
    glslang::GlslangToSpv(*vertexProgram->getIntermediate(EShLanguage::EShLangVertex), vertexSpv);

    vk::raii::Context context;
    vk::ApplicationInfo appInfo{"texture_renderer", 1, "texture_renderer", 1, VK_API_VERSION_1_1};
    vk::raii::Instance instance{context, vk::InstanceCreateInfo{{}, &appInfo}};

    vk::raii::PhysicalDevice physicalDevice{std::nullptr_t()};
    {
        vk::raii::PhysicalDevices physicalDevices{instance};
        physicalDevice = physicalDevices.at(0);
        for(const auto& device : physicalDevices)
        {
            const auto& props = device.getProperties();
            if(verbose) std::cout << "Device #" << props.deviceID << ": " << std::quoted(std::string(props.deviceName)) << " of type " << vk::to_string(props.deviceType) << '\n';
            if(props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu || props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
                physicalDevice = device;
        }
        if(verbose) std::cout << '\n';
    }
    
    const auto& props = physicalDevice.getProperties();
    std::cout << "Picked device #" << props.deviceID << ": " << std::quoted(std::string(props.deviceName)) << " of type " << vk::to_string(props.deviceType) << ".\n";
    const auto& memoryProperties = physicalDevice.getMemoryProperties();

    auto queues = physicalDevice.getQueueFamilyProperties();
    uint32_t queueFamilyIndex = std::distance(queues.begin(), std::find_if(queues.begin(), queues.end(), [](const vk::QueueFamilyProperties q){
        return (q.queueFlags & vk::QueueFlagBits::eGraphics) && (q.queueFlags & vk::QueueFlagBits::eTransfer);
    }));
    if(verbose) std::cout << "Using queue family " << queueFamilyIndex << ".\n";

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
            findMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
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
    vk::raii::Buffer transferBuffer{device, vk::BufferCreateInfo{
        {}, width*height*4, vk::BufferUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive
    }};
    auto memoryRequirements = transferBuffer.getMemoryRequirements();
    vk::raii::DeviceMemory transferMemory{device, vk::MemoryAllocateInfo{
        memoryRequirements.size,
        findMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
    }};
    transferBuffer.bindMemory(*transferMemory, 0);

    vk::raii::Buffer vertexBuffer{device, vk::BufferCreateInfo{
        {}, totalInputStride*100, vk::BufferUsageFlagBits::eVertexBuffer, vk::SharingMode::eExclusive
    }};
    memoryRequirements = transferBuffer.getMemoryRequirements();
    vk::raii::DeviceMemory vertexMemory{device, vk::MemoryAllocateInfo{
        memoryRequirements.size,
        findMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
    }};
    vertexBuffer.bindMemory(*vertexMemory, 0);
    {
        auto* ptr = vertexMemory.mapMemory(0, VK_WHOLE_SIZE);
        vertexMemory.unmapMemory();
    }

    std::vector<vk::AttachmentDescription> attachmentDescriptions{framebufferCount};
    std::fill(attachmentDescriptions.begin(), attachmentDescriptions.end(), vk::AttachmentDescription{
        {}, vk::Format::eR8G8B8A8Srgb, vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal
    });

    std::vector<vk::AttachmentReference> attachmentReferences{framebufferCount};
    std::generate(attachmentReferences.begin(), attachmentReferences.end(), [x = uint32_t{0}]() mutable{
        return vk::AttachmentReference{
            x++, vk::ImageLayout::eColorAttachmentOptimal
        };
    });
    vk::SubpassDescription subpass{{}, vk::PipelineBindPoint::eGraphics, {}, attachmentReferences, {}, {}, {}};
    vk::SubpassDependency externalDependency1{
        VK_SUBPASS_EXTERNAL, 0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead};
    vk::SubpassDependency externalDependency2{
        0, VK_SUBPASS_EXTERNAL,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eHost,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eHostRead};
    std::array deps = {externalDependency1, externalDependency2};
    vk::raii::RenderPass renderPass{device, vk::RenderPassCreateInfo{
        {}, attachmentDescriptions, subpass, deps
    }};

    std::vector<vk::ImageView> framebufferAttachments{framebufferCount};
    std::transform(framebufferImageViews.begin(), framebufferImageViews.end(), framebufferAttachments.begin(), [](vk::raii::ImageView& iv){return *iv;});
    vk::raii::Framebuffer framebuffer{device, vk::FramebufferCreateInfo{
        {}, *renderPass, framebufferAttachments, width, height, 1
    }};

    vk::raii::PipelineLayout pipelineLayout{device, vk::PipelineLayoutCreateInfo{/*TODO*/}};

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
    vk::Viewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    vk::Rect2D scissors{vk::Offset2D{}, vk::Extent2D{width, height}};
    vk::PipelineViewportStateCreateInfo pipelineViewportState{{}, viewport, scissors};
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
        &pipelineDepthStencilState, &pipelineColorBlendState, &pipelineDynamicState,
        *pipelineLayout, *renderPass, 0
    };
    vk::raii::Pipeline graphicsPipeline{device, std::nullptr_t(), pipelineCreateInfo};
    
    if(rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);

    vk::raii::CommandBuffer commandBuffer{std::move(buffers[0])};
    commandBuffer.begin(vk::CommandBufferBeginInfo{});
    std::vector<vk::ClearValue> clearValues{framebufferCount};
    std::fill(clearValues.begin(), clearValues.end(), vk::ClearValue{vk::ClearColorValue{0.0f, 0.0f, 0.0f, 0.0f}});
    commandBuffer.beginRenderPass(vk::RenderPassBeginInfo{*renderPass, *framebuffer, scissors, clearValues}, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
    commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
    commandBuffer.draw(1000, 1, 0, 0);
    commandBuffer.endRenderPass();
    commandBuffer.copyImageToBuffer(*framebufferImages[framebufferIndex], vk::ImageLayout::eTransferSrcOptimal, *transferBuffer, vk::BufferImageCopy{
        0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {0, 0, 0}, {width, height, 1}
    });
    commandBuffer.end();

    vk::raii::Fence fence{device, vk::FenceCreateInfo{}};
    queue.submit(vk::SubmitInfo{{}, {}, *commandBuffer}, *fence);

    auto result = device.waitForFences(*fence, true, UINT64_MAX);

    if(rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);
    
    {
        auto size = width*height*4;
        std::vector<char> copy(size);

        char* ptr = reinterpret_cast<char*>(transferMemory.mapMemory(0, VK_WHOLE_SIZE));
        std::copy(ptr, ptr+size, copy.begin());
        transferMemory.unmapMemory();

	    stbi_write_png(optionResult["output"].as<std::filesystem::path>().c_str(), width, height, 4, copy.data(), width*4);
    }
}
