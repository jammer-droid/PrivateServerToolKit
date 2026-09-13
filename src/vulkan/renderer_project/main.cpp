#include <vulkan/vulkan.h>

#include <iostream>

#include "common/VulkanException.h"
#include "common/VulkanHandle.h"

#include "core/VulkanContext.h"

#include "app/Window.h"

using SurfaceHandle = VulkanHandle<VkSurfaceKHR, deleter::VkSurfaceDeleter>;

int main()
{
    try
    {
        Window window;
        VulkanContext context(window.GetRequiredInstanceExtensions());
        SurfaceHandle surfaceHandle(window.CreateSurface(context.GetInstance()),
                                    deleter::VkSurfaceDeleter{context.GetInstance()});

        const PhysicalDeviceSelection selection = context.SelectPhysicalDevice(surfaceHandle.Get());
        VkPhysicalDeviceProperties selectedProperties{};
        vkGetPhysicalDeviceProperties(selection.physicalDevice, &selectedProperties);
        std::cout << "Selected GPU: " << selectedProperties.deviceName
                  << "\nGraphics family: " << selection.graphicsFamilyIndex
                  << "\nPresent family: " << selection.presentFamilyIndex
                  << "\nRequires portability subset: " << std::boolalpha << selection.requiresPortabilitySubset << '\n';

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
