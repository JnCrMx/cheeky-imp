#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

int main() {
    vk::raii::Context context;

    vk::ApplicationInfo appInfo("CheekyLayer", VK_MAKE_VERSION(0, 0, 1), "CheekyLayer", VK_MAKE_VERSION(0, 0, 1), VK_API_VERSION_1_2);

    vk::raii::Instance instance1(context, vk::InstanceCreateInfo({}, &appInfo, {}, {}));
    vk::raii::Instance instance2(context, vk::InstanceCreateInfo({}, &appInfo, {}, {}));
}
