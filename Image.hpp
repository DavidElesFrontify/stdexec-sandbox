#pragma once
#include <string>
class Image
{
    public:
        explicit Image(std::string name);
        void colorize();
        void resize();

        const std::string& getName() const;
    private:
        std::string m_name;
};

