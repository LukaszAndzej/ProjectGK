#pragma once

#include <exception>
#include <string>

class UnsupportedDedicatedPalette final : public std::exception
{
public:
    UnsupportedDedicatedPalette(const size_t numColors) : numColors{numColors},
                                                          message{
                                                              "Dedykowana paleta wspiera maksymalnie 16 kolorów (" + std::to_string(numColors) + " na obrazku)"
                                                          }
    {}

    char const* what() const noexcept(true) override
    {
        return message.c_str();
    }

private:
    size_t numColors;
    std::string message;
};
