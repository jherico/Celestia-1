// modelfile.h
//
// Copyright (C) 2004-2010, Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#pragma once

#include "model.h"
#include <string>

#define CEL_MODEL_HEADER_LENGTH 16
#define CEL_MODEL_HEADER_ASCII "#celmodel__ascii"
#define CEL_MODEL_HEADER_BINARY "#celmodel_binary"

namespace cmod {

/** Texture loading interface. Applications which want custom behavor for
  * texture loading should pass an instance of a TextureLoader subclass to
  * one of the model loading functions.
  */
class TextureLoader {
public:
    using Pointer = std::shared_ptr<TextureLoader>;
    using TextureResourcePtr = Material::TextureResource::Pointer;
    virtual ~TextureLoader(){};
    virtual TextureResourcePtr loadTexture(const std::string& name) = 0;
};

class ModelLoader {
public:
    using Pointer = std::shared_ptr<ModelLoader>;
    ModelLoader();
    virtual ~ModelLoader();

    virtual Model::Pointer load() = 0;

    const std::string& getErrorMessage() const;
    TextureLoader::Pointer getTextureLoader() const;
    void setTextureLoader(const TextureLoader::Pointer& _textureLoader);
    virtual VertexDataPointer loadVertices(const Mesh::VertexDescription& vertexDesc, unsigned int& vertexCount) = 0;

    static Pointer OpenModel(const std::string& filename);

protected:
    virtual void reportError(const std::string&);

private:
    std::string errorMessage;
    TextureLoader::Pointer textureLoader;
};

class ModelWriter {
public:
    virtual ~ModelWriter(){};

    virtual bool write(const Model&) = 0;
};

Model::Pointer LoadModel(const std::string& path, const TextureLoader::Pointer& textureLoader = nullptr);

#if 0
bool SaveModelAscii(const Model::Pointer& model, std::ostream& out);
bool SaveModelBinary(const Model::Pointer& model, std::ostream& out);
#endif
// Binary file tokens
enum ModelFileToken
{
    CMOD_Material = 1001,
    CMOD_EndMaterial = 1002,
    CMOD_Diffuse = 1003,
    CMOD_Specular = 1004,
    CMOD_SpecularPower = 1005,
    CMOD_Opacity = 1006,
    CMOD_Texture = 1007,
    CMOD_Mesh = 1009,
    CMOD_EndMesh = 1010,
    CMOD_VertexDesc = 1011,
    CMOD_EndVertexDesc = 1012,
    CMOD_Vertices = 1013,
    CMOD_Emissive = 1014,
    CMOD_Blend = 1015,
};

enum ModelFileType
{
    CMOD_Float1 = 1,
    CMOD_Float2 = 2,
    CMOD_Float3 = 3,
    CMOD_Float4 = 4,
    CMOD_String = 5,
    CMOD_Uint32 = 6,
    CMOD_Color = 7,
};

}  // namespace cmod

