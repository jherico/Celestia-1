// mesh.h
//
// Copyright (C) 2004-2010, Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELMODEL_MESH_H_
#define _CELMODEL_MESH_H_

#include "material.h"
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <vector>
#include <string>
#include <memory>

namespace cmod {

using VertexData = std::vector<uint8_t>;
using VertexDataPointer = std::shared_ptr<VertexData>;

using IndexData = std::vector<uint32_t>;
using IndexDataPointer = std::shared_ptr<IndexData>;


class Mesh {
public:
    using Pointer = std::shared_ptr<Mesh>;
    // 32-bit index type
    typedef uint32_t index32;
    class BufferResource {
    public:
        using Pointer = std::shared_ptr<BufferResource>;
    };

    enum VertexAttributeSemantic
    {
        Position = 0,
        Color0 = 1,
        Color1 = 2,
        Normal = 3,
        Tangent = 4,
        Texture0 = 5,
        Texture1 = 6,
        Texture2 = 7,
        Texture3 = 8,
        PointSize = 9,
        SemanticMax = 10,
        InvalidSemantic = -1,
    };

    enum VertexAttributeFormat
    {
        Float1 = 0,
        Float2 = 1,
        Float3 = 2,
        Float4 = 3,
        UByte4 = 4,
        FormatMax = 5,
        InvalidFormat = -1,
    };

    struct VertexAttribute {
        VertexAttribute() : semantic(InvalidSemantic), format(InvalidFormat), offset(0) {}

        VertexAttribute(VertexAttributeSemantic _semantic, VertexAttributeFormat _format, uint32_t _offset) :
            semantic(_semantic), format(_format), offset(_offset) {}

        VertexAttributeSemantic semantic;
        VertexAttributeFormat format;
        uint32_t offset;
    };

    using VertexAttributes = std::vector<VertexAttribute>;

    struct VertexDescription {
        using Pointer = std::shared_ptr<VertexDescription>;
        VertexDescription() {}
        VertexDescription(uint32_t _stride, const VertexAttributes& _attributes);
        VertexDescription(const VertexDescription& desc);
        ~VertexDescription();

        const VertexAttribute& getAttribute(VertexAttributeSemantic semantic) const { return semanticMap[semantic]; }

        bool validate() const;

        VertexDescription& operator=(const VertexDescription&);

        uint32_t stride{ 0 };
        VertexAttributes attributes;

    private:
        void clearSemanticMap();
        void buildSemanticMap();

        // Vertex attributes indexed by semantic
        std::array<VertexAttribute, SemanticMax> semanticMap;
    };

    enum PrimitiveGroupType
    {
        TriList = 0,
        TriStrip = 1,
        TriFan = 2,
        LineList = 3,
        LineStrip = 4,
        PointList = 5,
        SpriteList = 6,
        PrimitiveTypeMax = 7,
        InvalidPrimitiveGroupType = -1
    };

    class PrimitiveGroup {
    public:
        using Pointer = std::shared_ptr<PrimitiveGroup>;
        using ConstPointer = std::shared_ptr<const PrimitiveGroup>;
        PrimitiveGroup();
        ~PrimitiveGroup();

        uint32_t getPrimitiveCount() const;

        PrimitiveGroupType prim;
        uint32_t materialIndex;
        IndexData indices;
    };

    class PickResult {
    public:
        PickResult();

        Mesh::Pointer mesh;
        PrimitiveGroup::Pointer group;
        uint32_t primitiveIndex;
        double distance;
    };

    Mesh();
    ~Mesh();

    void setVertices(uint32_t _nVertices, const VertexDataPointer& vertexData);
    bool setVertexDescription(const VertexDescription& desc);
    const VertexDescription& getVertexDescription() const;

    PrimitiveGroup::ConstPointer getGroup(uint32_t index) const;
    PrimitiveGroup::Pointer getGroup(uint32_t index);
    uint32_t addGroup(const PrimitiveGroup::Pointer& group);
    uint32_t addGroup(PrimitiveGroupType prim, uint32_t materialIndex, IndexData& indices);
    uint32_t addGroup(PrimitiveGroupType prim, uint32_t materialIndex, const IndexData& indices);
    uint32_t getGroupCount() const;
    void remapIndices(const std::vector<index32>& indexMap);
    void clearGroups();

    void remapMaterials(const std::vector<uint32_t>& materialMap);

    /*! Reorder primitive groups so that groups with identical materials
     *  appear sequentially in the primitive group list. This will reduce
     *  the number of graphics state changes at render time.
     */
    void aggregateByMaterial();

    const std::string& getName() const;
    void setName(const std::string&);

    bool pick(const Eigen::Vector3d& origin, const Eigen::Vector3d& direction, PickResult& result) const;
    bool pick(const Eigen::Vector3d& origin, const Eigen::Vector3d& direction, double& distance) const;

    Eigen::AlignedBox<float, 3> getBoundingBox() const;
    void transform(const Eigen::Vector3f& translation, float scale);

    const void* getVertexData() const { return vertices->data(); }
    uint32_t getVertexCount() const { return nVertices; }
    uint32_t getVertexStride() const { return vertexDesc.stride; }
    uint32_t getPrimitiveCount() const;

    static PrimitiveGroupType parsePrimitiveGroupType(const std::string&);
    static VertexAttributeSemantic parseVertexAttributeSemantic(const std::string&);
    static VertexAttributeFormat parseVertexAttributeFormat(const std::string&);
    static Material::TextureSemantic parseTextureSemantic(const std::string&);
    static uint32_t getVertexAttributeSize(VertexAttributeFormat);

private:
    void recomputeBoundingBox();

private:
    VertexDescription vertexDesc;
    uint32_t nVertices{ 0 };
    VertexDataPointer vertices;
    mutable BufferResource::Pointer vbResource;

    std::vector<PrimitiveGroup::Pointer> groups;

    std::string name;
};

}  // namespace cmod

#endif  // !_CELMESH_MESH_H_
