#pragma once

#include "Image.hpp"

class Transform
{
    public:
        Image transform(const Image& image) const;
        Image transform_upper(Image image) const;
        Image transform_lower(Image image) const;
        Image combine(Image a, Image b) const;
};
