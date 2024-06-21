#include "Application.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <SDL2/SDL_syswm.h>
#include <unordered_map>

#include "Image.hpp"
#include "Logger.hpp"
#include "UnsupportedDedicatedPalette.hpp"

namespace
{
constexpr int openFileId = 1;
constexpr int openFile4BitId = 41;

constexpr int saveFileId = 2;
constexpr int saveFile4BitId = 42;

constexpr int closeFileId = 3;

constexpr int imposedPaletteTransformationId = 10;
constexpr int dedicatedPaletteTransformationId = 11;
constexpr int greyscaleTransformationId = 12;
constexpr int ditheringTransformationId = 13;
constexpr int ditheringGreyscaleTransformationId = 14;
constexpr int medianCutTransformationId = 15;
constexpr int medianCutGreyscaleTransformationId = 16;

const std::string kFileName = "obraz4.bin";

LRESULT CALLBACK WndProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
    Application* app;

    if (msg == WM_NCCREATE)
    {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        app = static_cast<Application*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));

        app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    else
    {
        app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (app)
    {
        return app->handleMenuBar(hwnd, msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void setPixel(const SDL_Surface* surface, const int x, const int y, const SDL_Color& color)
{
    if (x < 0 or x >= surface->w or y < 0 or y >= surface->h)
    {
        return;
    }

    const Uint32 pixel = SDL_MapRGB(surface->format, color.r, color.g, color.b);
    const int bpp = surface->format->BytesPerPixel;

    const auto p1 = static_cast<Uint8*>(surface->pixels) + y * 2 * surface->pitch + x * 2 * bpp;
    const auto p2 = static_cast<Uint8*>(surface->pixels) + (y * 2 + 1) * surface->pitch + x * 2 * bpp;
    const auto p3 = static_cast<Uint8*>(surface->pixels) + y * 2 * surface->pitch + (x * 2 + 1) * bpp;
    const auto p4 = static_cast<Uint8*>(surface->pixels) + (y * 2 + 1) * surface->pitch + (x * 2 + 1) * bpp;

    switch (bpp)
    {
        case 1:
            *p1 = pixel;
            *p2 = pixel;
            *p3 = pixel;
            *p4 = pixel;
            break;

        case 2:
            *reinterpret_cast<Uint16*>(p1) = pixel;
            *reinterpret_cast<Uint16*>(p2) = pixel;
            *reinterpret_cast<Uint16*>(p3) = pixel;
            *reinterpret_cast<Uint16*>(p4) = pixel;
            break;

        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
            {
                p1[0] = (pixel >> 16) & 0xff;
                p1[1] = (pixel >> 8) & 0xff;
                p1[2] = pixel & 0xff;
                p2[0] = (pixel >> 16) & 0xff;
                p2[1] = (pixel >> 8) & 0xff;
                p2[2] = pixel & 0xff;
                p3[0] = (pixel >> 16) & 0xff;
                p3[1] = (pixel >> 8) & 0xff;
                p3[2] = pixel & 0xff;
                p4[0] = (pixel >> 16) & 0xff;
                p4[1] = (pixel >> 8) & 0xff;
                p4[2] = pixel & 0xff;
            }
            else
            {
                p1[0] = pixel & 0xff;
                p1[1] = (pixel >> 8) & 0xff;
                p1[2] = (pixel >> 16) & 0xff;
                p2[0] = pixel & 0xff;
                p2[1] = (pixel >> 8) & 0xff;
                p2[2] = (pixel >> 16) & 0xff;
                p3[0] = pixel & 0xff;
                p3[1] = (pixel >> 8) & 0xff;
                p3[2] = (pixel >> 16) & 0xff;
                p4[0] = pixel & 0xff;
                p4[1] = (pixel >> 8) & 0xff;
                p4[2] = (pixel >> 16) & 0xff;
            }
            break;

        case 4:
            *reinterpret_cast<Uint32*>(p1) = pixel;
            *reinterpret_cast<Uint32*>(p2) = pixel;
            *reinterpret_cast<Uint32*>(p3) = pixel;
            *reinterpret_cast<Uint32*>(p4) = pixel;
            break;

        default:
            throw std::runtime_error("Not supported bpp");
    }
}
}

Application::Application()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        std::string error{"SDL_Init error: "};
        error += SDL_GetError();
        throw std::runtime_error(error);
    }

    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width * 2, height * 2, SDL_WINDOW_SHOWN);

    if (nullptr == window)
    {
        std::string error("SDL_CreateWindow error: ");
        error += SDL_GetError();
        throw std::runtime_error(error);
    }

    screen = SDL_GetWindowSurface(window);
    if (nullptr == screen)
    {
        std::string error("SDL_GetWindowSurface error: ");
        error += SDL_GetError();
        throw std::runtime_error(error);
    }

    initMenuBar();
}

Application::~Application()
{
    Logger::Log("Remove all .bin files");
    if (std::filesystem::exists(kFileName)) {
        std::error_code error;
        std::filesystem::remove(kFileName, error);
        Logger::Log((!error) ? ("Remove file: " + kFileName) : 
                            ("Can not remove file: " + kFileName + " (error: " + error.message() + ")"));
    }

    SDL_FreeSurface(screen);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Application::run()
{
    running = true;
    SDL_Event event;

    while (running and SDL_WaitEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT:
                running = false;
                break;

            default:
                break;
        }
    }
}

void Application::initMenuBar()
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    const HWND hwnd = wmInfo.info.win.window;

    SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    const HMENU hMenu = CreateMenu();

    HMENU hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, openFileId, "Wczytaj");
    AppendMenu(hFileMenu, MF_STRING, saveFileId, "Zapisz");
    AppendMenu(hFileMenu, MF_STRING, saveFile4BitId, "Zapisz 4-bit");
    AppendMenu(hFileMenu, MF_STRING, openFile4BitId, "Wczytaj 4-bit");
    AppendMenu(hFileMenu, MF_STRING, closeFileId, "Zamknij");
    AppendMenu(hMenu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(hFileMenu), "Obraz");

    HMENU hTransformMenu = CreatePopupMenu();
    AppendMenu(hTransformMenu, MF_STRING, imposedPaletteTransformationId, "Paleta narzucona");
    AppendMenu(hTransformMenu, MF_STRING, dedicatedPaletteTransformationId, "Paleta dedykowana");
    AppendMenu(hTransformMenu, MF_STRING, greyscaleTransformationId, "Skala szaro�ci");
    AppendMenu(hTransformMenu, MF_STRING, ditheringTransformationId, "Dithering");
    AppendMenu(hTransformMenu, MF_STRING, ditheringGreyscaleTransformationId, "Dithering Skala szaro�ci");
    AppendMenu(hTransformMenu, MF_STRING, medianCutTransformationId, "Median Cut");
    AppendMenu(hTransformMenu, MF_STRING, medianCutGreyscaleTransformationId, "Median Cut Skala szaro�ci");
    AppendMenu(hMenu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(hTransformMenu), "Transformacje");

    SetMenu(hwnd, hMenu);
}

LRESULT Application::handleMenuBar(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
    switch (msg)
    {
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case openFileId:
                    loadImage(hwnd);
                    break;

                case closeFileId:
                    closeImage();
                    break;

                case openFile4BitId:
                    OpenFile();
                    break;

                case saveFile4BitId:
                    SaveFile();
                    break;

                case imposedPaletteTransformationId:
                    if (image)
                    {
                        image->transform(Image::Transformation::imposedPalette);
                        updateView();
                    }
                    break;

                case dedicatedPaletteTransformationId:
                    if (image)
                    {
                        try
                        {
                            image->transform(Image::Transformation::dedicatedPalette);
                        }
                        catch (const UnsupportedDedicatedPalette& e)
                        {
                            MessageBox(hwnd, e.what(), "Error", MB_OK | MB_ICONERROR);
                        }
                        updateView();
                    }
                    break;

                case greyscaleTransformationId:
                    if (image)
                    {
                        image->transform(Image::Transformation::greyscale);
                        updateView();
                    }
                    break;

                case ditheringTransformationId:
                    if (image)
                    {
                        image->transform(Image::Transformation::dithering);
                        updateView();
                    }
                    break;

                case ditheringGreyscaleTransformationId:
                    if (image)
                    {
                        image->transform(Image::Transformation::ditheringGreyscale);
                        updateView();
                    }
                    break;

                case medianCutTransformationId:
                    if (image)
                    {
                        image->transform(Image::Transformation::medianCut);
                        updateView();
                    }
                    break;

                case medianCutGreyscaleTransformationId:
                    if (image)
                    {
                        image->transform(Image::Transformation::medianCutGreyscale);
                        updateView();
                    }
                    break;

                default:
                    break;
            }
            break;

        case WM_DESTROY:
            running = false;
            PostQuitMessage(0);
            break;

        default:
            break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Application::loadImage(const HWND hwnd)
{
    OPENFILENAME ofn;
    std::string fileName{};
    fileName.reserve(MAX_PATH);

    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Bitmaps\0*.BMP\0";
    ofn.lpstrFile = &fileName[0];
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "";

    if (GetOpenFileName(&ofn))
    {
        clearScreen();
        image = std::make_unique<Image>(fileName);
    }
    else
    {
        throw std::runtime_error("Failed to load image");
    }

    updateView();
}

void Application::saveImage(const HWND hwnd) const
{
    if (not image or not image->isTransformed())
    {
        MessageBox(hwnd, "Brak obrazu do zapisania", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    OPENFILENAME ofn;
    std::string fileName{};
    fileName.reserve(MAX_PATH);

    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "GK_PROJEKT_FILE\0*.gkimg\0";
    ofn.lpstrFile = &fileName[0];
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "gkimg";

    if (GetSaveFileName(&ofn))
    {
        std::ofstream file(fileName, std::ios::binary | std::ios::trunc);
        file << *image;
    }
}

void Application::closeImage()
{
    image = nullptr;
    updateView();
}

void Application::updateView() const
{
    if (not image)
    {
        clearScreen();
    }
    else
    {
        drawImage(image->getOriginalBmp(), 0, 0);
        if (const auto& transformedBmp = image->getTransformedBmp(); transformedBmp)
        {
            drawImage(transformedBmp.value(), static_cast<int>(image->getRows()), 0);
            drawPalette(image->getPalette(), 0, static_cast<int>(image->getColumns()) + 10);
        }
    }
    SDL_UpdateWindowSurface(window);
}

void Application::drawImage(const std::vector<std::vector<SDL_Color>>& imageData, const int x, const int y) const
{
    for (size_t i{0}; i < imageData.size(); ++i)
    {
        for (size_t j{0}; j < imageData[i].size(); ++j)
        {
            setPixel(screen, x + static_cast<int>(i), y + static_cast<int>(j), imageData[i][j]);
        }
    }
}

void Application::drawPalette(const std::array<SDL_Color, 16>& palette, const int x, const int y) const
{
    for (size_t i{0}; i < palette.size(); ++i)
    {
        const auto& color = palette[i];
        constexpr int paletteSize = 30;

        for (int j{0}; j < paletteSize; ++j)
        {
            for (int k{0}; k < paletteSize; ++k)
            {
                setPixel(screen, x + static_cast<int>(i) * paletteSize + j, y + k, color);
            }
        }
    }
}

void Application::clearScreen() const
{
    SDL_FillRect(screen, nullptr, SDL_MapRGB(screen->format, 0, 0, 0));
}

uint8_t Application::ConvertFrom24(const SDL_Color& color) {
    uint8_t r = color.r >> 6; // 2 most significant bits
    uint8_t g = color.g >> 6; // 2 most significant bits
    uint8_t b = color.b >> 6; // 2 most significant bits

    return (r << 4) | (g << 2) | b;
}

SDL_Color Application::ConvertTo24(uint8_t color_4_bit) {
    uint8_t r = (color_4_bit >> 4) & 0x03; // 2 most significant bits
    uint8_t g = (color_4_bit >> 2) & 0x03; // 2 most significant bits
    uint8_t b = color_4_bit & 0x03; // 2 most significant bits

    return SDL_Color{static_cast<uint8_t>(r << 6), static_cast<uint8_t>(g << 6), static_cast<uint8_t>(b << 6), 255};
}

void Application::SaveFile() {
    SDL_Color color;
    uint16_t image_width = width/2;
    uint16_t image_height = height/2;
    uint8_t color_4_bit;
    uint8_t bit_count = 4;
    char identifier[] = "DG";

    std::ofstream output(kFileName, std::ios::binary);
    output.write((char*)&identifier, sizeof(char) * 2);
    output.write((char*)&image_width, sizeof(uint16_t));
    output.write((char*)&image_height, sizeof(uint16_t));
    output.write((char*)&bit_count, sizeof(uint8_t));

    for (size_t y{0}; y < image_height; y++) {
        for (size_t x{0}; x < image_width; x++) {
            color = getPixel(x, y);
            color_4_bit = ConvertFrom24(color);
            output.write((char*)&color_4_bit, sizeof(uint8_t));
        }
    }

    output.close();
    SDL_UpdateWindowSurface(window);
}

void Application::OpenFile() {
    SDL_Color color;
    uint16_t image_width{0};
    uint16_t image_height{0};
    uint8_t n_bit_color{0};
    uint8_t bit_count{4};
    char identifier[2] = " ";

    std::ifstream input(kFileName, std::ios::binary);
    input.read((char*)&identifier, sizeof(char) * 2);
    input.read((char*)&image_width, sizeof(uint16_t));
    input.read((char*)&image_height, sizeof(uint16_t));
    input.read((char*)&bit_count, sizeof(uint8_t));

    for (size_t y{0}; y < image_height; y++) {
        for (size_t x{0}; x < image_width; x++) {
            input.read((char*)&n_bit_color, sizeof(uint8_t));
            color = ConvertTo24(n_bit_color);
            setPixel(screen, x + image_width, y, color);
        }
    }

    input.close();
    SDL_UpdateWindowSurface(window);
}

SDL_Color Application::getPixel(int x, int y) {
    SDL_Color color;
    Uint32 col = 0;
    if ((x >= 0) && (x < width) && (y >= 0) && (y < height)) {
        char* pPosition=(char*)screen->pixels;

        pPosition+=(screen->pitch*y*2);
        pPosition+=(screen->format->BytesPerPixel*x*2);

        memcpy(&col, pPosition, screen->format->BytesPerPixel);

        SDL_GetRGB(col, screen->format, &color.r, &color.g, &color.b);
    }
    return ( color );
}
