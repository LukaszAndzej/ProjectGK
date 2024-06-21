#include "Image.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include "FourBitColor.hpp"
#include "FourBitGrey.hpp"
#include "UnsupportedDedicatedPalette.hpp"

namespace
{
SDL_Color getColorFromSurface(const SDL_Surface* surface, const int x, const int y)
{
    if (x < 0 or x >= surface->w or y < 0 or y >= surface->h)
    {
        throw std::runtime_error("getColorFromSurface index out of bounds");
    }

    auto position = static_cast<char*>(surface->pixels);
    position += surface->pitch * y;
    position += surface->format->BytesPerPixel * x;

    Uint32 col;
    memcpy(&col, position, surface->format->BytesPerPixel);

    SDL_Color color;
    SDL_GetRGB(col, surface->format, &color.r, &color.g, &color.b);
    color.a = 1;
    return color;
}

bool compareSdlColor(const SDL_Color& lhs, const SDL_Color& rhs)
{
    return lhs.r == rhs.r and lhs.g == rhs.g and lhs.b == rhs.b;
}

constexpr size_t bayerTableSize = 4;

constexpr std::array<std::array<int, bayerTableSize>, bayerTableSize> bayerTable{
    std::array<int, bayerTableSize>{6, 14, 8, 16},
    std::array<int, bayerTableSize>{10, 2, 12, 4},
    std::array<int, bayerTableSize>{7, 15, 5, 13},
    std::array<int, bayerTableSize>{11, 3, 9, 1}
};

std::array<std::array<int, bayerTableSize>, bayerTableSize> getUpdatedBayerTable()
{
    constexpr int range{256};
    constexpr float factor{range * 1.0f / (bayerTableSize * bayerTableSize)};
    auto updateBayerTable = bayerTable;

    std::for_each(updateBayerTable.begin(), updateBayerTable.end(), [&factor](auto& row)
                      {
                          std::for_each(row.begin(), row.end(), [&factor](auto& value)
                                            {
                                                value = static_cast<int>((value * factor) - factor / 2);
                                            });
                      });

    return updateBayerTable;
}
}

Image::Image(const std::string& filepath) : transformedBmp{std::nullopt}, palette{}, currentTransformation{Transformation::none}
{
    SDL_Surface* bmp = SDL_LoadBMP(filepath.c_str());
    if (not bmp)
    {
        throw std::runtime_error("Failed to load bmp: " + filepath);
    }

    originalBmp.resize(bmp->w, std::vector<SDL_Color>(bmp->h));

    for (int x{0}; x < bmp->w; ++x)
    {
        for (int y{0}; y < bmp->h; ++y)
        {
            originalBmp[x][y] = getColorFromSurface(bmp, x, y);
        }
    }

    SDL_FreeSurface(bmp);
}

const std::vector<std::vector<SDL_Color>>& Image::getOriginalBmp() const
{
    return originalBmp;
}

const std::optional<std::vector<std::vector<SDL_Color>>>& Image::getTransformedBmp() const
{
    return transformedBmp;
}

const std::array<SDL_Color, 16>& Image::getPalette() const
{
    return palette;
}

size_t Image::getRows() const
{
    return originalBmp.size();
}

size_t Image::getColumns() const
{
    return originalBmp[0].size();
}

void Image::transform(const Transformation transformation)
{
    switch (transformation)
    {
        case Transformation::none:
            transformedBmp = std::nullopt;
            break;
        case Transformation::imposedPalette:
            imposedPaletteTransformation();
            break;

        case Transformation::dedicatedPalette:
            dedicatedPaletteTransformation();
            break;

        case Transformation::greyscale:
            greyscaleTransformation();
            break;

        case Transformation::dithering:
            ditheringTransformation();
            break;

        case Transformation::ditheringGreyscale:
            ditheringGreyscaleTransformation();
            break;

        case Transformation::medianCut:
            medianCutTransformation();
            break;

        case Transformation::medianCutGreyscale:
            medianCutGreyscaleTransformation();
            break;
    }
}

void Image::imposedPaletteTransformation()
{
    transformedBmp = originalBmp;
    for (auto& row : transformedBmp.value())
    {
        for (auto& pixel : row)
        {
            pixel = FourBitColor(pixel).getSdlColor();
        }
    }

    for (size_t i{0}; i < palette.size(); ++i)
    {
        palette[i] = FourBitColor{static_cast<Uint8>(i)}.getSdlColor();
    }

    currentTransformation = Transformation::imposedPalette;
}

void Image::dedicatedPaletteTransformation()
{
    std::vector<SDL_Color> dedicatedPalette;
    dedicatedPalette.reserve(originalBmp.size() * originalBmp[0].size());

    for (const auto& row : originalBmp)
    {
        for (const auto& pixel : row)
        {
            if (std::find_if(dedicatedPalette.begin(), dedicatedPalette.end(), [&pixel](const SDL_Color& color)
                                 {
                                     return compareSdlColor(color, pixel);
                                 }) == dedicatedPalette.end())
            {
                dedicatedPalette.emplace_back(pixel);
            }
        }
    }

    if (dedicatedPalette.size() > 16)
    {
        throw UnsupportedDedicatedPalette{dedicatedPalette.size()};
    }

    currentTransformation = Transformation::dedicatedPalette;
    clearPalette();
    std::copy_n(dedicatedPalette.begin(), dedicatedPalette.size(), palette.begin());
    transformedBmp = originalBmp;
}

void Image::greyscaleTransformation()
{
    transformedBmp = originalBmp;
    for (auto& row : transformedBmp.value())
    {
        for (auto& pixel : row)
        {
            pixel = FourBitGrey{pixel}.getSdlColor();
        }
    }

    for (size_t i{0}; i < palette.size(); ++i)
    {
        palette[i] = FourBitGrey{static_cast<Uint8>(i)}.getSdlColor();
    }

    currentTransformation = Transformation::greyscale;
}

void Image::ditheringTransformation()
{
    transformedBmp = originalBmp;
    const auto updatedBayerTable = getUpdatedBayerTable();
    for (size_t x{0}; x < transformedBmp.value().size(); ++x)
    {
        for (size_t y{0}; y < transformedBmp.value()[x].size(); ++y)
        {
            auto& pixel = transformedBmp.value()[x][y];
            pixel = FourBitColor(pixel).getSdlColor();

            const auto& updatedBayerTableValue = updatedBayerTable[y % bayerTableSize][x % bayerTableSize];

            if (pixel.r > updatedBayerTableValue)
            {
                pixel.r = 255;
            }
            else
            {}

            if (pixel.g > updatedBayerTableValue)
            {
                pixel.g = 255;
            }
            else
            {
                pixel.g = 0;
            }

            if (pixel.b > updatedBayerTableValue)
            {
                pixel.b = 255;
            }
            else
            {
                pixel.b = 0;
            }
        }
    }

    for (size_t i{0}; i < palette.size(); ++i)
    {
        palette[i] = FourBitColor{static_cast<Uint8>(i)}.getSdlColor();
    }

    currentTransformation = Transformation::dithering;
}

void Image::ditheringGreyscaleTransformation()
{
    transformedBmp = originalBmp;
    const auto updatedBayerTable = getUpdatedBayerTable();
    for (size_t x{0}; x < transformedBmp.value().size(); ++x)
    {
        for (size_t y{0}; y < transformedBmp.value()[x].size(); ++y)
        {
            auto& [r, g, b, a] = transformedBmp.value()[x][y];

            if (r > updatedBayerTable[y % bayerTableSize][x % bayerTableSize])
            {
                r = 255;
                g = 255;
                b = 255;
            }
            else
            {
                r = 0;
                g = 0;
                b = 0;
            }
        }
    }

    for (size_t i{0}; i < palette.size(); ++i)
    {
        palette[i] = FourBitGrey{static_cast<Uint8>(i)}.getSdlColor();
    }

    currentTransformation = Transformation::ditheringGreyscale;
}

void Image::medianCutTransformation()
{
    MedianCutter medianCutter{originalBmp, palette};
    transformedBmp = medianCutter.perform(false);
}

void Image::medianCutGreyscaleTransformation()
{
    MedianCutter medianCutter{originalBmp, palette};
    transformedBmp = medianCutter.perform(true);
}

void Image::clearPalette()
{
    std::fill(palette.begin(), palette.end(), SDL_Color{0, 0, 0, 0});
}

Image::MedianCutter::MedianCutter(const std::vector<std::vector<SDL_Color>>& image, std::array<SDL_Color, 16>& palette) : image{image},
    palette{palette},
    bucketsCount{0},
    colorsCount{0}
{
    colors.reserve(image.size() * image[0].size());
    greys.reserve(image.size() * image[0].size());

    for (const auto& row : image)
    {
        for (const auto& pixel : row)
        {
            colors.emplace_back(pixel);
            greys.emplace_back(static_cast<Uint8>(0.299 * pixel.r + 0.587 * pixel.g + 0.114 * pixel.b));
        }
    }
}

std::vector<std::vector<SDL_Color>> Image::MedianCutter::perform(const bool greyscale)
{
    if (greyscale)
    {
        return performGreyscale();
    }
    return performColor();
}

std::vector<std::vector<SDL_Color>> Image::MedianCutter::performColor()
{
    constexpr int iteration{4};

    medianCut(0, colors.size() - 1, iteration);

    auto transformedImage = image;

    for (auto& row : transformedImage)
    {
        for (auto& pixel : row)
        {
            pixel = palette[findNeighbour(pixel)];
        }
    }

    return transformedImage;
}

void Image::MedianCutter::medianCut(const size_t start, const size_t end, const int iteration)
{
    if (iteration > 0)
    {
        const SortBy sortBy = greatestDifference(start, end);
        sortBucket(start, end, sortBy);

        const size_t medium = (start + end + 1) / 2;

        medianCut(start, medium - 1, iteration - 1);
        medianCut(medium, end, iteration - 1);
        return;
    }

    int sumR{0}, sumG{0}, sumB{0};

    for (size_t p{start}; p <= end; ++p)
    {
        sumR += colors[p].r;
        sumG += colors[p].g;
        sumB += colors[p].b;
    }

    const int averageR = sumR / static_cast<int>((end - start + 1));
    const int averageG = sumG / static_cast<int>((end - start + 1));
    const int averageB = sumB / static_cast<int>((end - start + 1));

    palette[bucketsCount++] = SDL_Color{static_cast<Uint8>(averageR), static_cast<Uint8>(averageG), static_cast<Uint8>(averageB), 1};
}

Image::MedianCutter::SortBy Image::MedianCutter::greatestDifference(const size_t start, const size_t end) const
{
    size_t minR{start}, minG{start}, minB{start};
    size_t maxR{start}, maxG{start}, maxB{start};

    for (auto i{start}; i <= end; ++i)
    {
        if (colors[i].r < colors[minR].r)
        {
            minR = i;
        }
        if (colors[i].r > colors[maxR].r)
        {
            maxR = i;
        }

        if (colors[i].g < colors[minG].g)
        {
            minG = i;
        }
        if (colors[i].g > colors[maxG].g)
        {
            maxG = i;
        }

        if (colors[i].b < colors[minB].b)
        {
            minB = i;
        }
        if (colors[i].b > colors[maxB].b)
        {
            maxB = i;
        }
    }

    const auto differenceR = colors[maxR].r - colors[minR].r;
    const auto differenceG = colors[maxG].g - colors[minG].g;
    const auto differenceB = colors[maxB].b - colors[minB].b;

    const auto differenceMax = std::max(std::max(differenceR, differenceG), differenceB);

    if (differenceMax == differenceR)
    {
        return SortBy::red;
    }
    if (differenceMax == differenceG)
    {
        return SortBy::green;
    }
    return SortBy::blue;
}

void Image::MedianCutter::sortBucket(const size_t start, const size_t end, const SortBy sortBy)
{
    std::sort(colors.begin() + start, colors.begin() + end, [sortBy](const SDL_Color& lhs, const SDL_Color& rhs)
                  {
                      switch (sortBy)
                      {
                          case SortBy::red:
                              return lhs.r < rhs.r;
                          case SortBy::green:
                              return lhs.g < rhs.g;
                          case SortBy::blue:
                              return lhs.b < rhs.b;
                          default:
                              throw std::runtime_error("Invalid SortBy value");
                      }
                  });
}

size_t Image::MedianCutter::findNeighbour(const SDL_Color color) const
{
    double minimum{std::numeric_limits<double>::max()};
    size_t minimumIndex{};

    for (size_t i{0}; i < palette.size(); ++i)
    {
        const auto& [paletteColorR, paletteColorG, paletteColorB, _] = palette[i];

        if (const double distance = sqrt((color.r - paletteColorR) * (color.r - paletteColorR) +
                                         (color.g - paletteColorG) * (color.g - paletteColorG) +
                                         (color.b - paletteColorB) * (color.b - paletteColorB)); distance < minimum)
        {
            minimum = distance;
            minimumIndex = i;
        }
    }

    return minimumIndex;
}

std::vector<std::vector<SDL_Color>> Image::MedianCutter::performGreyscale()
{
    constexpr int iteration{4};

    medianCutGreyscale(0, greys.size() - 1, iteration);

    auto transformedImage = image;

    for (auto& row : transformedImage)
    {
        for (auto& pixel : row)
        {
            pixel = palette[findNeighbourGreyscale(pixel)];
        }
    }

    return transformedImage;
}

void Image::MedianCutter::medianCutGreyscale(const size_t start, const size_t end, const int iteration)
{
    if (iteration > 0)
    {
        sortBucketGreyscale(start, end);

        const size_t medium = (start + end + 1) / 2;

        medianCutGreyscale(start, medium - 1, iteration - 1);
        medianCutGreyscale(medium, end, iteration - 1);
        return;
    }

    int sumGrey{0};
    for (size_t p{start}; p <= end; ++p)
    {
        sumGrey += greys[p];
    }

    const Uint8 newGrey = sumGrey / static_cast<Uint8>((end - start + 1));
    palette[bucketsCount++] = SDL_Color{newGrey, newGrey, newGrey, 1};
}

void Image::MedianCutter::sortBucketGreyscale(const size_t start, const size_t end)
{
    std::sort(greys.begin() + start, greys.begin() + end);
}

size_t Image::MedianCutter::findNeighbourGreyscale(const SDL_Color color) const
{
    const Uint8 grey = static_cast<Uint8>(0.299 * color.r + 0.587 * color.g + 0.114 * color.b);
    int min{std::numeric_limits<int>::max()};
    size_t minIndex{};

    for (size_t i{0}; i < palette.size(); ++i)
    {
        if (const int distance = abs(grey - palette[i].r); distance < min)
        {
            min = distance;
            minIndex = i;
        }
    }

    return minIndex;
}

bool Image::isTransformed() const
{
    return transformedBmp.has_value();
}

std::ofstream& operator<<(std::ofstream& file, const Image& image)
{
    // TODO

    return file;
}
