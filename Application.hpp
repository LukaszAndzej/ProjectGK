#pragma once
#include <string>
#include <windows.h>
#include <memory>
#include <vector>
#include <SDL2/SDL.h>

class Image;

class Application
{
public:
    Application() noexcept(false);
    ~Application();

    void run();

    LRESULT handleMenuBar(HWND, UINT, WPARAM, LPARAM);

private:
    SDL_Window* window;
    SDL_Surface* screen;

    bool running{false};
    std::unique_ptr<Image> image;
    const int width{640};
    const int height{400};
    const std::string title{"GK2024 - Projekt - Zespol 24"};

    void initMenuBar();
    void loadImage(HWND);
    void saveImage(HWND) const;
    void closeImage();
    void updateView() const;
    void drawImage(const std::vector<std::vector<SDL_Color>>&, int, int) const;
    void drawPalette(const std::array<SDL_Color, 16>&, int, int) const;
    void clearScreen() const;

    uint8_t ConvertFrom24(const SDL_Color& color);
    SDL_Color ConvertTo24(uint8_t color_4_bit);

    void SaveFile();
    void OpenFile();

    SDL_Color getPixel(int x, int y);
};
