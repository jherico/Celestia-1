// modelfile.cpp
//
// Copyright (C) 2004-2010, Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "modelfile.h"
#include <celutil/basictypes.h>
#include <celutil/bytes.h>
#include <celutil/storage.hpp>
#include <cstring>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>

using namespace cmod;
using string = std::string;

struct IncrementalStorage {
    IncrementalStorage(const storage::StoragePointer& storage, size_t offset = 0) : storage(storage), offset(offset) {}

    IncrementalStorage(const std::string& filename) : IncrementalStorage(storage::Storage::readFile(filename)) {}

    using Pointer = std::shared_ptr<IncrementalStorage>;

    void read(char* dest, size_t size) {
        memcpy(dest, storage->data() + offset, size);
        offset += size;
    }

    void read(uint8_t* dest, size_t size) {
        memcpy(dest, storage->data() + offset, size);
        offset += size;
    }

    int get() {
        uint8_t nextChar = storage->data()[offset];
        ++offset;
        return nextChar;
    }

    bool eof() const { return offset >= storage->size(); }

    IncrementalStorage& ignore(size_t size) {
        offset += size;
        return *this;
    }

    size_t tellg() const { return offset; }

private:
    storage::StoragePointer storage;
    size_t offset{ 0 };
};

// Material default values
static Material::Color DefaultDiffuse(0.0f, 0.0f, 0.0f);
static Material::Color DefaultSpecular(0.0f, 0.0f, 0.0f);
static Material::Color DefaultEmissive(0.0f, 0.0f, 0.0f);
static float DefaultSpecularPower = 1.0f;
static float DefaultOpacity = 1.0f;
static Material::BlendMode DefaultBlend = Material::NormalBlend;

namespace cmod {

class Token {
public:
    enum TokenType
    {
        Name,
        String,
        Number,
        End,
        Invalid
    };

    Token() : m_type(Invalid), m_numberValue(0.0) {}

    Token(const Token& other) : m_type(other.m_type), m_numberValue(other.m_numberValue), m_stringValue(other.m_stringValue) {}

    Token& operator=(const Token& other) {
        m_type = other.m_type;
        m_numberValue = other.m_numberValue;
        m_stringValue = other.m_stringValue;
        return *this;
    }

    bool operator==(const Token& other) const {
        if (m_type == other.m_type) {
            switch (m_type) {
                case Name:
                case String:
                    return m_stringValue == other.m_stringValue;
                case Number:
                    return m_numberValue == other.m_numberValue;
                case End:
                case Invalid:
                    return true;
                default:
                    return false;
            }
        } else {
            return false;
        }
    }

    bool operator!=(const Token& other) const { return !(*this == other); }

    ~Token() {}

    TokenType type() const { return m_type; }

    bool isValid() const { return m_type != Invalid; }

    bool isNumber() const { return m_type == Number; }

    bool isInteger() const { return m_type == Number; }

    bool isName() const { return m_type == Name; }

    bool isString() const { return m_type == String; }

    double numberValue() const {
        assert(type() == Number);
        if (type() == Number) {
            return m_numberValue;
        } else {
            return 0.0;
        }
    }

    int integerValue() const {
        assert(type() == Number);
        //assert(std::floor(m_numberValue) == m_numberValue);

        if (type() == Number) {
            return (int)m_numberValue;
        } else {
            return 0;
        }
    }

    std::string stringValue() const {
        if (type() == Name || type() == String) {
            return m_stringValue;
        } else {
            return string();
        }
    }

public:
    static Token NumberToken(double value) {
        Token token;
        token.m_type = Number;
        token.m_numberValue = value;
        return token;
    }

    static Token NameToken(const string& value) {
        Token token;
        token.m_type = Name;
        token.m_stringValue = value;
        return token;
    }

    static Token StringToken(const string& value) {
        Token token;
        token.m_type = String;
        token.m_stringValue = value;
        return token;
    }

    static Token EndToken() {
        Token token;
        token.m_type = End;
        return token;
    }

private:
    TokenType m_type;
    double m_numberValue;
    string m_stringValue;
};

class TokenStream {
public:
    TokenStream(const IncrementalStorage::Pointer& in) :
        m_in(in), m_currentToken(), m_pushedBack(false), m_lineNumber(1), m_parseError(false), m_nextChar(' ') {}

    bool issep(char c) { return !isdigit(c) && !isalpha(c) && c != '.'; }

    void syntaxError(const std::string& message) {
        std::cerr << message << '\n';
        m_parseError = true;
    }

    Token nextToken();

    Token currentToken() const { return m_currentToken; }

    void pushBack() { m_pushedBack = true; }

    int readChar() {
        int c = (int)m_in->get();
        if (c == '\n') {
            m_lineNumber++;
        }

        return c;
    }

    int getLineNumber() const { return m_lineNumber; }

    bool hasError() const { return m_parseError; }

private:
    double numberFromParts(double integerValue,
                           double fractionValue,
                           double fracExp,
                           double exponentValue,
                           double exponentSign,
                           double sign) const {
        double x = integerValue + fractionValue / fracExp;
        if (exponentValue != 0) {
            x *= pow(10.0, exponentValue * exponentSign);
        }

        return x * sign;
    }

    enum State
    {
        StartState = 0,
        NameState = 1,
        NumberState = 2,
        FractionState = 3,
        ExponentState = 4,
        ExponentFirstState = 5,
        DotState = 6,
        CommentState = 7,
        StringState = 8,
        ErrorState = 9,
        StringEscapeState = 10,
    };

private:
    IncrementalStorage::Pointer m_in;
    Token m_currentToken;
    bool m_pushedBack;
    int m_lineNumber;
    bool m_parseError;
    int m_nextChar;
};

}  // namespace cmod

Token TokenStream::nextToken() {
    State state = StartState;

    if (m_pushedBack) {
        m_pushedBack = false;
        return m_currentToken;
    }

    if (m_currentToken.type() == Token::End) {
        // Already at end of stream
        return m_currentToken;
    }

    double integerValue = 0;
    double fractionValue = 0;
    double sign = 1;
    double fracExp = 1;
    double exponentValue = 0;
    double exponentSign = 1;

    Token newToken;

    string textValue;

    while (!hasError() && !newToken.isValid()) {
        switch (state) {
            case StartState:
                if (isspace(m_nextChar)) {
                    state = StartState;
                } else if (isdigit(m_nextChar)) {
                    state = NumberState;
                    integerValue = m_nextChar - (int)'0';
                } else if (m_nextChar == '-') {
                    state = NumberState;
                    sign = -1;
                    integerValue = 0;
                } else if (m_nextChar == '+') {
                    state = NumberState;
                    sign = +1;
                    integerValue = 0;
                } else if (m_nextChar == '.') {
                    state = FractionState;
                    sign = +1;
                    integerValue = 0;
                } else if (isalpha(m_nextChar) || m_nextChar == '_') {
                    state = NameState;
                    textValue += (char)m_nextChar;
                } else if (m_nextChar == '#') {
                    state = CommentState;
                } else if (m_nextChar == '"') {
                    state = StringState;
                } else if (m_nextChar == -1) {
                    newToken = Token::EndToken();
                } else {
                    syntaxError("Bad character in stream");
                }
                break;

            case NameState:
                if (isalpha(m_nextChar) || isdigit(m_nextChar) || m_nextChar == '_') {
                    state = NameState;
                    textValue += (char)m_nextChar;
                } else {
                    newToken = Token::NameToken(textValue);
                }
                break;

            case CommentState:
                if (m_nextChar == '\n' || m_nextChar == '\r' || m_nextChar == std::char_traits<char>::eof()) {
                    state = StartState;
                }
                break;

            case StringState:
                if (m_nextChar == '"') {
                    newToken = Token::StringToken(textValue);
                    m_nextChar = readChar();
                } else if (m_nextChar == '\\') {
                    state = StringEscapeState;
                } else if (m_nextChar == std::char_traits<char>::eof()) {
                    syntaxError("Unterminated string");
                } else {
                    state = StringState;
                    textValue += (char)m_nextChar;
                }
                break;

            case StringEscapeState:
                if (m_nextChar == '\\') {
                    textValue += '\\';
                    state = StringState;
                } else if (m_nextChar == 'n') {
                    textValue += '\n';
                    state = StringState;
                } else if (m_nextChar == '"') {
                    textValue += '"';
                    state = StringState;
                } else {
                    syntaxError("Unknown escape code in string");
                    state = StringState;
                }
                break;

            case NumberState:
                if (isdigit(m_nextChar)) {
                    state = NumberState;
                    integerValue = integerValue * 10 + m_nextChar - (int)'0';
                } else if (m_nextChar == '.') {
                    state = FractionState;
                } else if (m_nextChar == 'e' || m_nextChar == 'E') {
                    state = ExponentFirstState;
                } else if (issep(m_nextChar)) {
                    double x = numberFromParts(integerValue, fractionValue, fracExp, exponentValue, exponentSign, sign);
                    newToken = Token::NumberToken(x);
                } else {
                    syntaxError("Bad character in number");
                }
                break;

            case FractionState:
                if (isdigit(m_nextChar)) {
                    state = FractionState;
                    fractionValue = fractionValue * 10 + m_nextChar - (int)'0';
                    fracExp *= 10;
                } else if (m_nextChar == 'e' || m_nextChar == 'E') {
                    state = ExponentFirstState;
                } else if (issep(m_nextChar)) {
                    double x = numberFromParts(integerValue, fractionValue, fracExp, exponentValue, exponentSign, sign);
                    newToken = Token::NumberToken(x);
                } else {
                    syntaxError("Bad character in number");
                }
                break;

            case ExponentFirstState:
                if (isdigit(m_nextChar)) {
                    state = ExponentState;
                    exponentValue = (int)m_nextChar - (int)'0';
                } else if (m_nextChar == '-') {
                    state = ExponentState;
                    exponentSign = -1;
                } else if (m_nextChar == '+') {
                    state = ExponentState;
                } else {
                    state = ErrorState;
                    syntaxError("Bad character in number");
                }
                break;

            case ExponentState:
                if (isdigit(m_nextChar)) {
                    state = ExponentState;
                    exponentValue = exponentValue * 10 + (int)m_nextChar - (int)'0';
                } else if (issep(m_nextChar)) {
                    double x = numberFromParts(integerValue, fractionValue, fracExp, exponentValue, exponentSign, sign);
                    newToken = Token::NumberToken(x);
                } else {
                    state = ErrorState;
                    syntaxError("Bad character in number");
                }
                break;

            case DotState:
                if (isdigit(m_nextChar)) {
                    state = FractionState;
                    fractionValue = fractionValue * 10 + (int)m_nextChar - (int)'0';
                    fracExp = 10;
                } else {
                    state = ErrorState;
                    syntaxError("'.' in stupid place");
                }
                break;

            case ErrorState:
                break;  // Prevent GCC4 warnings; do nothing

        }  // Switch

        if (!hasError() && !newToken.isValid()) {
            m_nextChar = readChar();
        }
    }

    m_currentToken = newToken;

    return newToken;
}

/*!
This is an approximate Backus Naur form for the contents of ASCII cmod
files. For brevity, the categories &lt;unsigned_int&gt; and &lt;float&gt; aren't
defined here--they have the obvious definitions.
\code
<modelfile>           ::= <header> <model>

<header>              ::= #celmodel__ascii

<model>               ::= { <material_definition> } { <mesh_definition> }

<material_definition> ::= material
                          { <material_attribute> }
                          end_material

<texture_semantic>    ::= texture0       |
                          normalmap      |
                          specularmap    |
                          emissivemap

<texture>             ::= <texture_semantic> <string>

<material_attribute>  ::= diffuse <color>   |
                          specular <color>  |
                          emissive <color>  |
                          specpower <float> |
                          opacity <float>   |
                          blend <blendmode> |
                          <texture>

<color>               ::= <float> <float> <float>

<string>              ::= """ { letter } """

<blendmode>           ::= normal | add | premultiplied

<mesh_definition>     ::= mesh
                          <vertex_description>
                          <vertex_pool>
                          { <prim_group> }
                          end_mesh

<vertex_description>  ::= vertexdesc
                          { <vertex_attribute> }
                          end_vertexdesc

<vertex_attribute>    ::= <vertex_semantic> <vertex_format>

<vertex_semantic>     ::= position | normal | color0 | color1 | tangent |
                          texcoord0 | texcoord1 | texcoord2 | texcoord3 |
                          pointsize

<vertex_format>       ::= f1 | f2 | f3 | f4 | ub4

<vertex_pool>         ::= vertices <count>
                          { <float> }

<count>               ::= <unsigned_int>

<prim_group>          ::= <prim_group_type> <material_index> <count>
                          { <unsigned_int> }

<prim_group_type>     ::= trilist | tristrip | trifan |
                          linelist | linestrip | points |
                          sprites

<material_index>      :: <unsigned_int> | -1
\endcode
*/
class AsciiModelLoader : public ModelLoader {
public:
    AsciiModelLoader(const IncrementalStorage::Pointer& _in);
    ~AsciiModelLoader();

    Model::Pointer load() override;
    void reportError(const string&) override;
    VertexDataPointer loadVertices(const Mesh::VertexDescription& vertexDesc, unsigned int& vertexCount) override;

    Material::Pointer loadMaterial();
    Mesh::VertexDescription::Pointer loadVertexDescription();
    Mesh::Pointer loadMesh();

private:
    TokenStream tok;
};

// Standard tokens for ASCII model loader
static Token MeshToken = Token::NameToken("mesh");
static Token EndMeshToken = Token::NameToken("end_mesh");
static Token VertexDescToken = Token::NameToken("vertexdesc");
static Token EndVertexDescToken = Token::NameToken("end_vertexdesc");
static Token VerticesToken = Token::NameToken("vertices");
static Token MaterialToken = Token::NameToken("material");
static Token EndMaterialToken = Token::NameToken("end_material");

class BinaryModelLoader : public ModelLoader {
public:
    BinaryModelLoader(const IncrementalStorage::Pointer& _in);
    ~BinaryModelLoader();

    virtual Model::Pointer load();
    virtual void reportError(const string&);

    Material::Pointer loadMaterial();
    Mesh::VertexDescription::Pointer loadVertexDescription();
    Mesh::Pointer loadMesh();
    VertexDataPointer loadVertices(const Mesh::VertexDescription& vertexDesc, unsigned int& vertexCount) override;

private:
    IncrementalStorage::Pointer inPtr;
    IncrementalStorage& in;
};

ModelLoader::ModelLoader() {
}

ModelLoader::~ModelLoader() {
}

void ModelLoader::reportError(const string& msg) {
    errorMessage = msg;
}

const string& ModelLoader::getErrorMessage() const {
    return errorMessage;
}

void ModelLoader::setTextureLoader(const TextureLoader::Pointer& _textureLoader) {
    textureLoader = _textureLoader;
}

TextureLoader::Pointer ModelLoader::getTextureLoader() const {
    return textureLoader;
}

Model::Pointer cmod::LoadModel(const std::string& filename, const TextureLoader::Pointer& textureLoader) {
    ModelLoader::Pointer loader = ModelLoader::OpenModel(filename);
    if (loader == nullptr)
        return nullptr;

    loader->setTextureLoader(textureLoader);

    auto model = loader->load();
    if (!model) {
        std::cerr << "Error in model file: " << loader->getErrorMessage() << '\n';
    }

    return model;
}

ModelLoader::Pointer ModelLoader::OpenModel(const std::string& filename) {
    auto in = std::make_shared<IncrementalStorage>(filename);
    char header[CEL_MODEL_HEADER_LENGTH + 1];
    memset(header, '\0', sizeof(header));
    in->read(header, CEL_MODEL_HEADER_LENGTH);
    if (strcmp(header, CEL_MODEL_HEADER_ASCII) == 0) {
        return std::make_shared<AsciiModelLoader>(in);
    } else if (strcmp(header, CEL_MODEL_HEADER_BINARY) == 0) {
        return std::make_shared<BinaryModelLoader>(in);
    } else {
        std::cerr << "Model file has invalid header.\n";
        return nullptr;
    }
}

AsciiModelLoader::AsciiModelLoader(const IncrementalStorage::Pointer& _in) : tok(_in) {
}

AsciiModelLoader::~AsciiModelLoader() {
}

void AsciiModelLoader::reportError(const string& msg) {
    char buf[32];
    sprintf(buf, " (line %d)", tok.getLineNumber());
    ModelLoader::reportError(msg + string(buf));
}

Material::Pointer AsciiModelLoader::loadMaterial() {
    if (tok.nextToken() != MaterialToken) {
        reportError("Material definition expected");
        return nullptr;
    }

    Material::Pointer material = std::make_shared<Material>();

    material->diffuse = DefaultDiffuse;
    material->specular = DefaultSpecular;
    material->emissive = DefaultEmissive;
    material->specularPower = DefaultSpecularPower;
    material->opacity = DefaultOpacity;

    while (tok.nextToken().isName() && tok.currentToken() != EndMaterialToken) {
        string property = tok.currentToken().stringValue();
        Material::TextureSemantic texType = Mesh::parseTextureSemantic(property);

        if (texType != Material::InvalidTextureSemantic) {
            Token t = tok.nextToken();
            if (t.type() != Token::String) {
                reportError("Texture name expected");
                return nullptr;
            }

            string textureName = t.stringValue();

            Material::TextureResource::Pointer tex;
            if (getTextureLoader()) {
                tex = getTextureLoader()->loadTexture(textureName);
            } else {
                tex = std::make_shared<Material::DefaultTextureResource>(textureName);
            }

            material->maps[texType] = tex;
        } else if (property == "blend") {
            Material::BlendMode blendMode = Material::InvalidBlend;

            Token t = tok.nextToken();
            if (t.isName()) {
                string blendModeName = tok.currentToken().stringValue();
                if (blendModeName == "normal")
                    blendMode = Material::NormalBlend;
                else if (blendModeName == "add")
                    blendMode = Material::AdditiveBlend;
                else if (blendModeName == "premultiplied")
                    blendMode = Material::PremultipliedAlphaBlend;
            }

            if (blendMode == Material::InvalidBlend) {
                reportError("Bad blend mode in material");
                return nullptr;
            }

            material->blend = blendMode;
        } else {
            // All non-texture material properties are 3-vectors except
            // specular power and opacity
            double data[3];
            int nValues = 3;
            if (property == "specpower" || property == "opacity") {
                nValues = 1;
            }

            for (int i = 0; i < nValues; i++) {
                Token t = tok.nextToken();
                if (t.type() != Token::Number) {
                    reportError("Bad property value in material");
                    return nullptr;
                }
                data[i] = t.numberValue();
            }

            Material::Color colorVal;
            if (nValues == 3) {
                colorVal = Material::Color((float)data[0], (float)data[1], (float)data[2]);
            }

            if (property == "diffuse") {
                material->diffuse = colorVal;
            } else if (property == "specular") {
                material->specular = colorVal;
            } else if (property == "emissive") {
                material->emissive = colorVal;
            } else if (property == "opacity") {
                material->opacity = (float)data[0];
            } else if (property == "specpower") {
                material->specularPower = (float)data[0];
            }
        }
    }

    if (tok.currentToken().type() != Token::Name) {
        return nullptr;
    }

    return material;
}

Mesh::VertexDescription::Pointer AsciiModelLoader::loadVertexDescription() {
    if (tok.nextToken() != VertexDescToken) {
        reportError("Vertex description expected");
        return nullptr;
    }

    static constexpr int maxAttributes = 16;
    unsigned int offset = 0;
    Mesh::VertexAttributes attributes;

    while (tok.nextToken().isName() && tok.currentToken() != EndVertexDescToken) {
        string semanticName;
        string formatName;
        bool valid = false;

        if (attributes.size() == maxAttributes) {
            // TODO: Should eliminate the attribute limit, though no real vertex
            // will ever exceed it.
            reportError("Attribute limit exceeded in vertex description");
            return nullptr;
        }

        semanticName = tok.currentToken().stringValue();

        if (tok.nextToken().isName()) {
            formatName = tok.currentToken().stringValue();
            valid = true;
        }

        if (!valid) {
            reportError("Invalid vertex description");
            return nullptr;
        }

        Mesh::VertexAttributeSemantic semantic = Mesh::parseVertexAttributeSemantic(semanticName);
        if (semantic == Mesh::InvalidSemantic) {
            reportError(string("Invalid vertex attribute semantic '") + semanticName + "'");
            return nullptr;
        }

        Mesh::VertexAttributeFormat format = Mesh::parseVertexAttributeFormat(formatName);
        if (format == Mesh::InvalidFormat) {
            reportError(string("Invalid vertex attribute format '") + formatName + "'");
            return nullptr;
        }

        attributes.emplace_back(semantic, format, offset);
        offset += Mesh::getVertexAttributeSize(format);
    }

    if (tok.currentToken().type() != Token::Name) {
        reportError("Invalid vertex description");
        return nullptr;
    }

    if (attributes.empty()) {
        reportError("Vertex definitition cannot be empty");
        return nullptr;
    }

    return std::make_shared<Mesh::VertexDescription>(offset, attributes);
}

VertexDataPointer AsciiModelLoader::loadVertices(const Mesh::VertexDescription& vertexDesc, unsigned int& vertexCount) {
    if (tok.nextToken() != VerticesToken) {
        reportError("Vertex data expected");
        return nullptr;
    }

    if (tok.nextToken().type() != Token::Number) {
        reportError("Vertex count expected");
        return nullptr;
    }

    double num = tok.currentToken().numberValue();
    if (num != floor(num) || num <= 0.0) {
        reportError("Bad vertex count for mesh");
        return nullptr;
    }

    vertexCount = (unsigned int)num;
    unsigned int vertexDataSize = vertexDesc.stride * vertexCount;
    VertexDataPointer result = std::make_shared<VertexData>();
    result->resize(vertexDataSize);
    auto vertexData = result->data();

    unsigned int offset = 0;
    double data[4];
    for (unsigned int i = 0; i < vertexCount; i++, offset += vertexDesc.stride) {
        assert(offset < vertexDataSize);
        for (const auto& attribute : vertexDesc.attributes) {
            const auto& fmt = attribute.format;
            int readCount = 0;
            switch (fmt) {
                case Mesh::Float1:
                    readCount = 1;
                    break;
                case Mesh::Float2:
                    readCount = 2;
                    break;
                case Mesh::Float3:
                    readCount = 3;
                    break;
                case Mesh::Float4:
                case Mesh::UByte4:
                    readCount = 4;
                    break;
                default:
                    assert(0);
                    return nullptr;
            }

            for (int j = 0; j < readCount; j++) {
                if (!tok.nextToken().isNumber()) {
                    reportError("Error in vertex data");
                    data[j] = 0.0;
                } else {
                    data[j] = tok.currentToken().numberValue();
                }
                // TODO: range check unsigned byte values
            }

            unsigned int base = offset + attribute.offset;
            if (fmt == Mesh::UByte4) {
                for (int k = 0; k < readCount; k++) {
                    reinterpret_cast<unsigned char*>(vertexData + base)[k] = (unsigned char)(data[k]);
                }
            } else {
                for (int k = 0; k < readCount; k++)
                    reinterpret_cast<float*>(vertexData + base)[k] = (float)data[k];
            }
        }
    }

    return result;
}

Mesh::Pointer AsciiModelLoader::loadMesh() {
    if (tok.nextToken() != MeshToken) {
        reportError("Mesh definition expected");
        return nullptr;
    }

    auto vertexDesc = loadVertexDescription();
    if (!vertexDesc)
        return nullptr;

    unsigned int vertexCount = 0;
    auto vertexData = loadVertices(*vertexDesc, vertexCount);
    if (!vertexData) {
        return nullptr;
    }

    Mesh::Pointer mesh = std::make_shared<Mesh>();
    mesh->setVertexDescription(*vertexDesc);
    mesh->setVertices(vertexCount, vertexData);

    while (tok.nextToken().isName() && tok.currentToken() != EndMeshToken) {
        Mesh::PrimitiveGroupType type = Mesh::parsePrimitiveGroupType(tok.currentToken().stringValue());
        if (type == Mesh::InvalidPrimitiveGroupType) {
            reportError("Bad primitive group type: " + tok.currentToken().stringValue());
            return nullptr;
        }

        if (!tok.nextToken().isInteger()) {
            reportError("Material index expected in primitive group");
            return nullptr;
        }

        unsigned int materialIndex;
        if (tok.currentToken().integerValue() == -1) {
            materialIndex = ~0u;
        } else {
            materialIndex = (unsigned int)tok.currentToken().integerValue();
        }

        if (!tok.nextToken().isInteger()) {
            reportError("Index count expected in primitive group");
            return nullptr;
        }

        unsigned int indexCount = (unsigned int)tok.currentToken().integerValue();

        IndexData indices;
        indices.resize(indexCount);
        for (unsigned int i = 0; i < indexCount; i++) {
            if (!tok.nextToken().isInteger()) {
                reportError("Incomplete index list in primitive group");
                return nullptr;
            }

            unsigned int index = (unsigned int)tok.currentToken().integerValue();
            if (index >= vertexCount) {
                reportError("Index out of range");
                return nullptr;
            }

            indices[i] = index;
        }

        mesh->addGroup(type, materialIndex, indices);
    }

    return mesh;
}

Model::Pointer AsciiModelLoader::load() {
    Model::Pointer model = std::make_shared<Model>();
    bool seenMeshes = false;

    if (model == nullptr) {
        reportError("Unable to allocate memory for model");
        return nullptr;
    }

    // Parse material and mesh definitions
    for (Token token = tok.nextToken(); token.type() != Token::End; token = tok.nextToken()) {
        if (token.isName()) {
            string name = tok.currentToken().stringValue();
            tok.pushBack();

            if (name == "material") {
                if (seenMeshes) {
                    reportError("Materials must be defined before meshes");
                    return nullptr;
                }

                Material::Pointer material = loadMaterial();
                if (!material) {
                    return nullptr;
                }

                model->addMaterial(material);
            } else if (name == "mesh") {
                seenMeshes = true;

                auto mesh = loadMesh();
                if (!mesh) {
                    return nullptr;
                }

                model->addMesh(mesh);
            } else {
                reportError(string("Error: Unknown block type ") + name);
                return nullptr;
            }
        } else {
            reportError("Block name expected");
            return nullptr;
        }
    }

    return model;
}

/***** Binary loader *****/

BinaryModelLoader::BinaryModelLoader(const IncrementalStorage::Pointer& _in) : inPtr(_in), in(*inPtr) {
}

BinaryModelLoader::~BinaryModelLoader() {
}

void BinaryModelLoader::reportError(const string& msg) {
    char buf[32];
    sprintf(buf, " (offset %d)", 0);
    ModelLoader::reportError(msg + string(buf));
}

// Read a big-endian 32-bit unsigned integer
static int32 readUint(IncrementalStorage& in) {
    int32 ret;
    in.read((char*)&ret, sizeof(int32));
    LE_TO_CPU_INT32(ret, ret);
    return (uint32)ret;
}

static float readFloat(IncrementalStorage& in) {
    float f;
    in.read((char*)&f, sizeof(float));
    LE_TO_CPU_FLOAT(f, f);
    return f;
}

static int16 readInt16(IncrementalStorage& in) {
    int16 ret;
    in.read((char*)&ret, sizeof(int16));
    LE_TO_CPU_INT16(ret, ret);
    return ret;
}

static ModelFileToken readToken(IncrementalStorage& in) {
    return (ModelFileToken)readInt16(in);
}

static ModelFileType readType(IncrementalStorage& in) {
    return (ModelFileType)readInt16(in);
}

static bool readTypeFloat1(IncrementalStorage& in, float& f) {
    if (readType(in) != CMOD_Float1)
        return false;
    f = readFloat(in);
    return true;
}

static bool readTypeColor(IncrementalStorage& in, Material::Color& c) {
    if (readType(in) != CMOD_Color)
        return false;

    float r = readFloat(in);
    float g = readFloat(in);
    float b = readFloat(in);
    c = Material::Color(r, g, b);

    return true;
}

static bool readTypeString(IncrementalStorage& in, string& s) {
    if (readType(in) != CMOD_String)
        return false;

    uint16 len;
    in.read((char*)&len, sizeof(uint16));
    LE_TO_CPU_INT16(len, len);

    if (len == 0) {
        s = "";
    } else {
        char* buf = new char[len];
        in.read(buf, len);
        s = string(buf, len);
        delete[] buf;
    }

    return true;
}

static bool ignoreValue(IncrementalStorage& in) {
    ModelFileType type = readType(in);
    int size = 0;

    switch (type) {
        case CMOD_Float1:
            size = 4;
            break;
        case CMOD_Float2:
            size = 8;
            break;
        case CMOD_Float3:
            size = 12;
            break;
        case CMOD_Float4:
            size = 16;
            break;
        case CMOD_Uint32:
            size = 4;
            break;
        case CMOD_Color:
            size = 12;
            break;
        case CMOD_String: {
            uint16 len;
            in.read((char*)&len, sizeof(uint16));
            LE_TO_CPU_INT16(len, len);
            size = len;
        } break;

        default:
            return false;
    }

    in.ignore(size);

    return true;
}

Model::Pointer BinaryModelLoader::load() {
    auto model = std::make_shared<Model>();
    bool seenMeshes = false;

    if (model == nullptr) {
        reportError("Unable to allocate memory for model");
        return nullptr;
    }

    // Parse material and mesh definitions
    for (;;) {
        ModelFileToken tok = readToken(in);

        if (in.eof()) {
            break;
        } else if (tok == CMOD_Material) {
            if (seenMeshes) {
                reportError("Materials must be defined before meshes");
                return nullptr;
            }

            auto material = loadMaterial();
            if (!material) {
                return nullptr;
            }

            model->addMaterial(material);
        } else if (tok == CMOD_Mesh) {
            seenMeshes = true;

            auto mesh = loadMesh();
            if (!mesh) {
                return nullptr;
            }

            model->addMesh(mesh);
        } else {
            reportError("Error: Unknown block type in model");
            return nullptr;
        }
    }

    return model;
}

Material::Pointer BinaryModelLoader::loadMaterial() {
    auto material = std::make_shared<Material>();

    material->diffuse = DefaultDiffuse;
    material->specular = DefaultSpecular;
    material->emissive = DefaultEmissive;
    material->specularPower = DefaultSpecularPower;
    material->opacity = DefaultOpacity;

    for (;;) {
        ModelFileToken tok = readToken(in);
        switch (tok) {
            case CMOD_Diffuse:
                if (!readTypeColor(in, material->diffuse)) {
                    reportError("Incorrect type for diffuse color");
                    return nullptr;
                }
                break;

            case CMOD_Specular:
                if (!readTypeColor(in, material->specular)) {
                    reportError("Incorrect type for specular color");
                    return nullptr;
                }
                break;

            case CMOD_Emissive:
                if (!readTypeColor(in, material->emissive)) {
                    reportError("Incorrect type for emissive color");
                    return nullptr;
                }
                break;

            case CMOD_SpecularPower:
                if (!readTypeFloat1(in, material->specularPower)) {
                    reportError("Float expected for specularPower");
                    return nullptr;
                }
                break;

            case CMOD_Opacity:
                if (!readTypeFloat1(in, material->opacity)) {
                    reportError("Float expected for opacity");
                    return nullptr;
                }
                break;

            case CMOD_Blend: {
                int16 blendMode = readInt16(in);
                if (blendMode < 0 || blendMode >= Material::BlendMax) {
                    reportError("Bad blend mode");
                    return nullptr;
                }
                material->blend = (Material::BlendMode)blendMode;
            } break;

            case CMOD_Texture: {
                int16 texType = readInt16(in);
                if (texType < 0 || texType >= Material::TextureSemanticMax) {
                    reportError("Bad texture type");
                    return nullptr;
                }

                string texfile;
                if (!readTypeString(in, texfile)) {
                    reportError("String expected for texture filename");
                    return nullptr;
                }

                if (texfile.empty()) {
                    reportError("Zero length texture name in material definition");
                    return nullptr;
                }

                Material::TextureResource::Pointer tex;
                auto texLoader = getTextureLoader();
                if (texLoader) {
                    tex = texLoader->loadTexture(texfile);
                } else {
                    tex = std::make_shared<Material::DefaultTextureResource>(texfile);
                }

                material->maps[texType] = tex;
            } break;

            case CMOD_EndMaterial:
                return material;

            default:
                // Skip unrecognized tokens
                if (!ignoreValue(in)) {
                    return nullptr;
                }
        }  // switch
    }      // for
}

Mesh::VertexDescription::Pointer BinaryModelLoader::loadVertexDescription() {
    if (readToken(in) != CMOD_VertexDesc) {
        reportError("Vertex description expected");
        return nullptr;
    }

    static const size_t maxAttributes = 16;
    unsigned int offset = 0;
    Mesh::VertexAttributes attributes;

    for (;;) {
        int16 tok = readInt16(in);

        if (tok == CMOD_EndVertexDesc) {
            break;
        } else if (tok >= 0 && tok < Mesh::SemanticMax) {
            Mesh::VertexAttributeFormat fmt = static_cast<Mesh::VertexAttributeFormat>(readInt16(in));
            if (fmt < 0 || fmt >= Mesh::FormatMax) {
                reportError("Invalid vertex attribute type");
                return nullptr;
            } else {
                if (attributes.size() == maxAttributes) {
                    reportError("Too many attributes in vertex description");
                    return nullptr;
                }

                attributes.emplace_back(static_cast<Mesh::VertexAttributeSemantic>(tok), fmt, offset);
                offset += Mesh::getVertexAttributeSize(fmt);
            }
        } else {
            reportError("Invalid semantic in vertex description");
            return nullptr;
        }
    }

    if (attributes.empty()) {
        reportError("Vertex definitition cannot be empty");
        return nullptr;
    }

    return std::make_shared<Mesh::VertexDescription>(offset, attributes);
}

Mesh::Pointer BinaryModelLoader::loadMesh() {
    auto vertexDesc = loadVertexDescription();
    if (!vertexDesc) {
        return nullptr;
    }

    unsigned int vertexCount = 0;
    auto vertexData = loadVertices(*vertexDesc, vertexCount);
    if (!vertexData) {
        return nullptr;
    }

    auto mesh = std::make_shared<Mesh>();
    mesh->setVertexDescription(*vertexDesc);
    mesh->setVertices(vertexCount, vertexData);

    for (;;) {
        int16 tok = readInt16(in);

        if (tok == CMOD_EndMesh) {
            break;
        } else if (tok < 0 || tok >= Mesh::PrimitiveTypeMax) {
            reportError("Bad primitive group type");
            return nullptr;
        }

        Mesh::PrimitiveGroupType type = static_cast<Mesh::PrimitiveGroupType>(tok);
        unsigned int materialIndex = readUint(in);
        unsigned int indexCount = readUint(in);

        IndexData indices;
        indices.resize(indexCount);

        for (unsigned int i = 0; i < indexCount; i++) {
            uint32 index = readUint(in);
            if (index >= vertexCount) {
                reportError("Index out of range");
                return nullptr;
            }

            indices[i] = index;
        }
        mesh->addGroup(type, materialIndex, indices);
    }

    return mesh;
}

VertexDataPointer BinaryModelLoader::loadVertices(const Mesh::VertexDescription& vertexDesc, unsigned int& vertexCount) {
    if (readToken(in) != CMOD_Vertices) {
        reportError("Vertex data expected");
        return nullptr;
    }

    vertexCount = readUint(in);
    unsigned int vertexDataSize = vertexDesc.stride * vertexCount;
    auto result = std::make_shared<VertexData>();
    result->resize(vertexDataSize);
    auto vertexData = result->data();

    unsigned int offset = 0;
    for (unsigned int i = 0; i < vertexCount; i++, offset += vertexDesc.stride) {
        assert(offset < vertexDataSize);
        for (const auto& attr : vertexDesc.attributes) {
            unsigned int base = offset + attr.offset;
            Mesh::VertexAttributeFormat fmt = attr.format;
            /*int readCount = 0;    Unused*/
            switch (fmt) {
                case Mesh::Float1:
                    reinterpret_cast<float*>(vertexData + base)[0] = readFloat(in);
                    break;
                case Mesh::Float2:
                    reinterpret_cast<float*>(vertexData + base)[0] = readFloat(in);
                    reinterpret_cast<float*>(vertexData + base)[1] = readFloat(in);
                    break;
                case Mesh::Float3:
                    reinterpret_cast<float*>(vertexData + base)[0] = readFloat(in);
                    reinterpret_cast<float*>(vertexData + base)[1] = readFloat(in);
                    reinterpret_cast<float*>(vertexData + base)[2] = readFloat(in);
                    break;
                case Mesh::Float4:
                    reinterpret_cast<float*>(vertexData + base)[0] = readFloat(in);
                    reinterpret_cast<float*>(vertexData + base)[1] = readFloat(in);
                    reinterpret_cast<float*>(vertexData + base)[2] = readFloat(in);
                    reinterpret_cast<float*>(vertexData + base)[3] = readFloat(in);
                    break;
                case Mesh::UByte4:
                    in.read(reinterpret_cast<char*>(vertexData + base), 4);
                    break;
                default:
                    assert(0);
                    return nullptr;
            }
        }
    }
    return result;
}

#ifdef CMOD_LOAD_TEST

int main(int argc, char* argv[]) {
    Model* model = LoadModel(cin);
    if (model) {
        SaveModelAscii(model, cout);
    }

    return 0;
}

#endif
