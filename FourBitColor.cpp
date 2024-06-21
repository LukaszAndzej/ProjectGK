#include "FourBitColor.hpp"
#include <cmath>

FourBitColor::FourBitColor(const SDL_Color& color) : r{static_cast<Uint8>(round(color.r * 3.0 / 255.0))},
                                                     g{static_cast<Uint8>(round(color.g * 1.0 / 255.0))},
                                                     b{static_cast<Uint8>(round(color.b * 1.0 / 255.0))}
{}

FourBitColor::FourBitColor(const Uint8 color) : r{static_cast<Uint8>((color & 0b00001100) >> 2)},
                                                g{static_cast<Uint8>((color & 0b00000010) >> 1)},
                                                b{static_cast<Uint8>(color & 0b00000001)}
{}

SDL_Color FourBitColor::getSdlColor() const
{
    return SDL_Color{static_cast<Uint8>(r * 255.0 / 3.0), static_cast<Uint8>(g * 255.0), static_cast<Uint8>(b * 255.0), 1};
}
