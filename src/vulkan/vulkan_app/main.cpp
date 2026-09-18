#include "app/WorldSandbox.h"
#include "runtime/app/Application.h"

#include <cstdlib>
#include <exception>
#include <iostream>

int main()
{
    try
    {
        WorldSandbox game;
        const ApplicationConfig config{RENDERER_SHADER_DIR};
        Application application(config, game);
        application.Run();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
