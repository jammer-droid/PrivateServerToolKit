#include <vulkan/vulkan.h>

#include <iostream>
#include <cstdint>

#include "common/VulkanException.h"

#include "core/VulkanContext.h"

int main()
{
    try
    {
        std::uint32_t apiVersion = VulkanContext::GetApiVersion();
        std::cout << VK_API_VERSION_MAJOR(apiVersion) << '.' << VK_API_VERSION_MINOR(apiVersion) << '.'
                  << VK_API_VERSION_PATCH(apiVersion) << '\n';

        VulkanContext context;
    }
    catch (const VulkanException &error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    };

    return 0;
}
