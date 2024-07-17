#include "Transformator.hpp"

Image Transform::transform(Image image) const
{
    image.colorize();
    image.resize();
    return image;
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
