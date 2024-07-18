#include "Transformator.hpp"

#include <optick.h>

Image Transform::transform(const Image& image) const
{
    OPTICK_EVENT();
    Image result = image;
    result.colorize();
    result.resize();
    return result;
}

Image Transform::transform_upper(Image image) const
{
    image.colorize();
    return image;
}

Image Transform::transform_lower(Image image) const
{
    image.resize();
    return image;
}

Image Transform::combine(Image a, Image b) const
{
    return a;
}
