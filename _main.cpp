#include "Application.hpp"
#include <iostream>

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    try
    {
        Application app;
        app.run();
        return EXIT_SUCCESS;
    }
    catch (std::runtime_error& ex)
    {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
