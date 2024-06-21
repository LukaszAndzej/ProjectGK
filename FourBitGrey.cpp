#include "FourBitGrey.hpp"

FourBitGrey::FourBitGrey(const SDL_Color& color) : grey{static_cast<Uint8>((0.299 * color.r + 0.587f * color.g + 0.114f * color.b) * 15.0 / 255.0)}
{}

FourBitGrey::FourBitGrey(const Uint8 color) : grey{color}
{}

SDL_Color FourBitGrey::getSdlColor() const
{
    return SDL_Color{static_cast<Uint8>(grey * 255.0 / 15.0), static_cast<Uint8>(grey * 255.0 / 15.0), static_cast<Uint8>(grey * 255.0 / 15.0), 1};
}
