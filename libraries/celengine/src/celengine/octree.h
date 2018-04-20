// octree.h
//
// Octree-based visibility determination for objects.
//
// Copyright (C) 2001-2009, Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_OCTREE_H_
#define _CELENGINE_OCTREE_H_

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <celmath/plane.h>
#include "observer.h"
#include <vector>
#include <array>

// The DynamicOctree and StaticOctree template arguments are:
// OBJ:  object hanging from the node,
// PREC: floating point precision of the culling operations at node level.
// The hierarchy of octree nodes is built using a single precision value (excludingFactor), which relates to an
// OBJ's limiting property defined by the octree particular specialization: ie. we use [absolute magnitude] for star octrees, etc.
// For details, see notes below.

template <class OBJ, class PREC>
class OctreeProcessor {
public:
    OctreeProcessor(){};
    virtual ~OctreeProcessor(){};

    virtual void process(const std::shared_ptr<OBJ>& obj, PREC distance, float appMag) = 0;
};

struct OctreeLevelStatistics {
    uint32_t nodeCount;
    uint32_t objectCount;
    double size;
};

template <class OBJ, class PREC>
class StaticOctree;

template <class OBJ, class PREC>
class DynamicOctree {
public:
    using Pointer = std::shared_ptr<DynamicOctree>;
    using PointType = Eigen::Matrix<PREC, 3, 1>;

private:
    using ObjectPtr = std::shared_ptr<OBJ>;
    using ObjectList = std::vector<ObjectPtr>;
    using ObjectListItr = typename ObjectList::const_iterator;

    //using Static = typename StaticOctree<OBJ, PREC>;
    //using StaticPtr = ;
    //using StaticPtrArray = std::array<StaticPtr, 8>;

    using LimitingFactorPredicate = std::function<bool(const ObjectPtr&, const float)>;
    using StraddlingPredicate = std::function<bool(const PointType&, const ObjectPtr&, const float)>;
    using ExclusionFactorDecayFunction = std::function<PREC(const PREC)>;

public:
    DynamicOctree(const PointType& cellCenterPos, const float exclusionFactor) :
        cellCenterPos(cellCenterPos), exclusionFactor(exclusionFactor) {}

    ~DynamicOctree() {}

    void insertObject(const ObjectPtr& obj, const PREC scale) {
        // If the object can't be placed into this node's children, then put it here:
        if (limitingFactorPredicate(obj, exclusionFactor) || straddlingPredicate(cellCenterPos, obj, exclusionFactor))
            add(obj);
        else {
            // If we haven't allocated child nodes yet, try to fit
            // the object in this node, even though it could be put
            // in a child. Only if there are more than SPLIT_THRESHOLD
            // objects in the node will we attempt to place the
            // object into a child node.  This is done in order
            // to avoid having the octree degenerate into one object
            // per node.
            if (!_children) {
                // Make sure that there's enough room left in this node
                if (_objects.size() >= DynamicOctree<OBJ, PREC>::SPLIT_THRESHOLD)
                    split(scale * (PREC)0.5f);
                add(obj);
            } else {
                // We've already allocated child nodes; place the object
                // into the appropriate one.
                this->getChild(obj, cellCenterPos)->insertObject(obj, scale * (PREC)0.5);
            }
        }
    }

    void rebuildAndSort(std::shared_ptr<StaticOctree<OBJ, PREC>>& outStaticNode, ObjectList& outSortedObjects) {
        outStaticNode = std::make_shared<StaticOctree<OBJ, PREC>>(cellCenterPos, exclusionFactor, _objects);
        outSortedObjects.insert(outSortedObjects.end(), _objects.begin(), _objects.end());
        if (_children) {
            auto& children = *_children;
            auto& staticNode = *outStaticNode;
            auto& staticNodeChildren = staticNode._children;
            staticNodeChildren.reset(new std::array<std::shared_ptr<StaticOctree<OBJ, PREC>>, 8>());
            for (int i = 0; i < 8; ++i) {
                const auto& child = (*_children)[i];
                auto& outChild = (*outStaticNode->_children)[i];
                child->rebuildAndSort(outChild, outSortedObjects);
            }
        }
    }

private:
    static uint32_t SPLIT_THRESHOLD;

    static LimitingFactorPredicate limitingFactorPredicate;
    static StraddlingPredicate straddlingPredicate;
    static ExclusionFactorDecayFunction decayFunction;

private:
    void add(const ObjectPtr& obj) { _objects.push_back(obj); }

    void split(const PREC scale) {
        _children.reset(new std::array<DynamicOctree::Pointer, 8>);

        for (int i = 0; i < 8; ++i) {
            Eigen::Matrix<PREC, 3, 1> centerPos = cellCenterPos;

            centerPos += Eigen::Matrix<PREC, 3, 1>(((i & XPos) != 0) ? scale : -scale, ((i & YPos) != 0) ? scale : -scale,
                                                   ((i & ZPos) != 0) ? scale : -scale);

#if 0
            centerPos.x += ((i & XPos) != 0) ? scale : -scale;
            centerPos.y += ((i & YPos) != 0) ? scale : -scale;
            centerPos.z += ((i & ZPos) != 0) ? scale : -scale;
#endif

            (*_children)[i] = std::make_shared<DynamicOctree>(centerPos, decayFunction(exclusionFactor));
        }
        sortIntoChildNodes();
    }

    // Sort this node's objects into objects that can remain here,
    // and objects that should be placed into one of the eight
    // child nodes.
    void sortIntoChildNodes() {
        size_t nKeptInParent = 0;

        for (const auto& obj : _objects) {
            if (limitingFactorPredicate(obj, exclusionFactor) || straddlingPredicate(cellCenterPos, obj, exclusionFactor)) {
                _objects[nKeptInParent++] = obj;
            } else {
                getChild(obj, cellCenterPos)->add(obj);
            }
        }
        _objects.resize(nKeptInParent);
    }

    Pointer getChild(const ObjectPtr&, const PointType&);

    std::unique_ptr<std::array<Pointer, 8>> _children;
    PointType cellCenterPos;
    PREC exclusionFactor;
    ObjectList _objects;
};

template <class OBJ, class PREC>
class StaticOctree {
    friend class DynamicOctree<OBJ, PREC>;

public:
    using PointType = Eigen::Matrix<PREC, 3, 1>;
    using Pointer = std::shared_ptr<StaticOctree>;
    using ObjectPtr = std::shared_ptr<OBJ>;
    using ObjectList = std::vector<ObjectPtr>;

public:
    StaticOctree(const PointType& cellCenterPos, const float exclusionFactor, const ObjectList& objects) :
        cellCenterPos(cellCenterPos), exclusionFactor(exclusionFactor), _objects(objects) {}

    ~StaticOctree() {}

    // These methods are only declared at the template level; we'll implement them as
    // full specializations, allowing for different traversal strategies depending on the
    // object type and nature.

    // This method searches the octree for objects that are likely to be visible
    // to a viewer with the specified obsPosition and limitingFactor.  The
    // octreeProcessor is invoked for each potentially visible object --no object with
    // a property greater than limitingFactor will be processed, but
    // objects that are outside the view frustum may be.  Frustum tests are performed
    // only at the node level to optimize the octree traversal, so an exact test
    // (if one is required) is the responsibility of the callback method.
    void processVisibleObjects(OctreeProcessor<OBJ, PREC>& processor,
                               const PointType& obsPosition,
                               const Eigen::Hyperplane<PREC, 3>* frustumPlanes,
                               float limitingFactor,
                               PREC scale) const;

    void processCloseObjects(OctreeProcessor<OBJ, PREC>& processor,
                             const PointType& obsPosition,
                             PREC boundingRadius,
                             PREC scale) const;

    size_t countChildren() const {
        size_t count = 0;
        if (_children) {
            for (const auto& child : *_children) {
                count += 1 + child->countChildren();
            }
        }
        return count;
    }

    size_t countObjects() const {
        size_t count = _objects.size();
        if (_children) {
            for (const auto& child : *_children) {
                count += child->countObjects();
            }
        }
        return count;
    }

    void computeStatistics(std::vector<OctreeLevelStatistics>& stats, uint32_t level = 0) {
        if (level >= stats.size()) {
            while (level >= stats.size()) {
                OctreeLevelStatistics levelStats;
                levelStats.nodeCount = 0;
                levelStats.objectCount = 0;
                levelStats.size = 0.0;
                stats.push_back(levelStats);
            }
        }

        stats[level].nodeCount++;
        stats[level].objectCount += _objects.size();
        stats[level].size = 0.0;

        if (_children) {
            for (const auto& child : *_children) {
                child->computeStatistics(stats, level + 1);
            }
        }
    }

private:
    static const PREC SQRT3;

private:
    std::unique_ptr<std::array<Pointer, 8>> _children;
    PointType cellCenterPos;
    float exclusionFactor;
    std::vector<ObjectPtr> _objects;
    //OBJ* _firstObject;
    //uint32_t nObjects;
};

// There are two classes implemented in this module: StaticOctree and
// DynamicOctree.  The DynamicOctree is built first by inserting
// objects from a database or catalog and is then 'compiled' into a StaticOctree.
// In the process of building the StaticOctree, the original object database is
// reorganized, with objects in the same octree node all placed adjacent to each
// other.  This spatial sorting of the objects dramatically improves the
// performance of octree operations through much more coherent memory access.
enum
{
    XPos = 1,
    YPos = 2,
    ZPos = 4,
};

// The SPLIT_THRESHOLD is the number of objects a node must contain before its
// children are generated. Increasing this number will decrease the number of
// octree nodes in the tree, which will use less memory but make culling less
// efficient.

//MS VC++ wants this to be placed here:
template <class OBJ, class PREC>
const PREC StaticOctree<OBJ, PREC>::SQRT3 = (PREC)1.732050807568877;


template <class OBJ, class PREC>

#endif  // _OCTREE_H_
