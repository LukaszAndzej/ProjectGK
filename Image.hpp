#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>
#include <SDL2/SDL.h>

class Image
{
public:
    enum class Transformation
    {
        none,
        imposedPalette,
        dedicatedPalette,
        greyscale,
        dithering,
        ditheringGreyscale,
        medianCut,
        medianCutGreyscale,
    };

    explicit Image(const std::string&);

    void transform(Transformation);

    size_t getRows() const;
    size_t getColumns() const;

    bool isTransformed() const;

    const std::vector<std::vector<SDL_Color>>& getOriginalBmp() const;
    const std::optional<std::vector<std::vector<SDL_Color>>>& getTransformedBmp() const;
    const std::array<SDL_Color, 16>& getPalette() const;

    friend std::ofstream& operator<<(std::ofstream&, const Image&);

private:
    std::vector<std::vector<SDL_Color>> originalBmp;
    std::optional<std::vector<std::vector<SDL_Color>>> transformedBmp;
    std::array<SDL_Color, 16> palette;
    Transformation currentTransformation;

    class MedianCutter
    {
    public:
        MedianCutter(const std::vector<std::vector<SDL_Color>>& image, std::array<SDL_Color, 16>& palette);

        std::vector<std::vector<SDL_Color>> perform(bool);

    private:
        enum class SortBy
        {
            red,
            green,
            blue,
        };

        const std::vector<std::vector<SDL_Color>>& image;
        std::array<SDL_Color, 16>& palette;
        int bucketsCount;
        int colorsCount;
        std::vector<SDL_Color> colors;
        std::vector<Uint8> greys;

        std::vector<std::vector<SDL_Color>> performGreyscale();
        void medianCutGreyscale(size_t, size_t, int);
        void sortBucketGreyscale(size_t, size_t);
        size_t findNeighbourGreyscale(SDL_Color) const;

        std::vector<std::vector<SDL_Color>> performColor();
        size_t findNeighbour(SDL_Color) const;
        void medianCut(size_t, size_t, int);
        SortBy greatestDifference(size_t, size_t) const;
        void sortBucket(size_t, size_t, SortBy);
    };

    void imposedPaletteTransformation();
    void dedicatedPaletteTransformation() noexcept(false);
    void greyscaleTransformation();
    void ditheringTransformation();
    void ditheringGreyscaleTransformation();
    void medianCutTransformation();
    void medianCutGreyscaleTransformation();
    void clearPalette();
};
