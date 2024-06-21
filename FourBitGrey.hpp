#pragma once
#include <SDL2/SDL.h>

class FourBitGrey
{
public:
    explicit FourBitGrey(const SDL_Color&);
    explicit FourBitGrey(Uint8);

    SDL_Color getSdlColor() const;

private:
    Uint8 grey;
};
