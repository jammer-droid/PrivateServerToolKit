#include <vulkan/vulkan.h>

#include <iostream>

#include "common/VulkanException.h"

#include "core/VulkanContext.h"

#include "app/Window.h"

int main()
{
    try
    {
        Window window;
        VulkanContext context;

        while (!window.IsCloseRequested())
        {
            window.WaitEvents();
        }
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
