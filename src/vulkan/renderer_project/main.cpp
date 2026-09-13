#include <vulkan/vulkan.h>

#include <iostream>
#include <cstdint>

#include "common/VulkanException.h"

#include "core/VulkanContext.h"

int main()
{
    try
    {
        VulkanContext context;
    }
    catch (const VulkanException &error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (const std::runtime_error &error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return 0;
}
