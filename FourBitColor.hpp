#pragma once
#include <SDL2/SDL.h>

class FourBitColor
{
public:
    explicit FourBitColor(const SDL_Color&);
    explicit FourBitColor(Uint8);

    SDL_Color getSdlColor() const;

private:
    Uint8 r;
    Uint8 g;
    Uint8 b;
};
