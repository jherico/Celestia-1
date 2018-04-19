// material.h
//
// Copyright (C) 2004-2010, Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELMODEL_MATERIAL_H_
#define _CELMODEL_MATERIAL_H_

#include <Eigen/Core>
#include <string>
#include <memory>
#include <array>

namespace cmod {

class Material {
public:
    using Pointer = std::shared_ptr<Material>;
    using UniquePointer = std::unique_ptr<Material>;

    Material();
    ~Material();

    class Color {
    public:
        Color() {}

        Color(float r, float g, float b) : m_red(r), m_green(g), m_blue(b) {}

        float red() const { return m_red; }

        float green() const { return m_green; }

        float blue() const { return m_blue; }

        Eigen::Vector3f toVector3() const { return Eigen::Vector3f(m_red, m_green, m_blue); }

        bool operator==(const Color& other) const {
            return m_red == other.m_red && m_green == other.m_green && m_blue == other.m_blue;
        }

        bool operator!=(const Color& other) const { return !(*this == other); }

    private:
        float m_red{ 0.0f };
        float m_green{ 0.0f };
        float m_blue{ 0.0f };
    };

    class TextureResource {
    public:
        using Pointer = std::shared_ptr<TextureResource>;
        virtual ~TextureResource(){};
        virtual std::string source() const = 0;
    };

    class DefaultTextureResource : public TextureResource {
    public:
        DefaultTextureResource(const std::string& source) : m_source(source){};
        std::string source() const { return m_source; }

    private:
        std::string m_source;
    };

    enum BlendMode
    {
        NormalBlend = 0,
        AdditiveBlend = 1,
        PremultipliedAlphaBlend = 2,
        BlendMax = 3,
        InvalidBlend = -1,
    };

    enum TextureSemantic
    {
        DiffuseMap = 0,
        NormalMap = 1,
        SpecularMap = 2,
        EmissiveMap = 3,
        TextureSemanticMax = 4,
        InvalidTextureSemantic = -1,
    };

    Color diffuse;
    Color emissive;
    Color specular;
    float specularPower{ 1.0f };
    float opacity{ 1.0f };
    BlendMode blend{ BlendMode::NormalBlend };
    std::array<TextureResource::Pointer, TextureSemanticMax> maps;
};

}  // namespace cmod

#endif  // _CELMODEL_MATERIAL_H_
