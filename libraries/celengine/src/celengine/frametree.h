// frametree.h
//
// Reference frame tree.
//
// Copyright (C) 2008, the Celestia Development Team
// Initial version by Chris Laurel, claurel@gmail.com
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_FRAMETREE_H_
#define _CELENGINE_FRAMETREE_H_

#include <vector>
#include <cstddef>
#include "forward.h"

class FrameTree {
public:
    FrameTree(const StarPtr&);
    FrameTree(const BodyPtr&);
    ~FrameTree();

    /*! Return the star that this tree is associated with; it will be
     *  NULL for frame trees associated with solar system bodies.
     */
    const StarPtr& getStar() const { return starParent; }

    const ReferenceFramePtr& getDefaultReferenceFrame() const;

    void addChild(const TimelinePhasePtr& phase);
    void removeChild(const TimelinePhasePtr& phase);
    const TimelinePhasePtr& getChild(size_t n) const;
    size_t childCount() const;

    void markChanged();
    void markUpdated();
    void recomputeBoundingSphere();

    bool isRoot() const { return bodyParent == NULL; }

    bool updateRequired() const { return m_changed; }

    /*! Get the radius of a sphere large enough to contain all
     *  objects in the tree.
     */
    double boundingSphereRadius() const { return m_boundingSphereRadius; }

    /*! Get the radius of the largest body in the tree.
     */
    double maxChildRadius() const { return m_maxChildRadius; }

    /*! Return whether any of the children of this frame
     *  are secondary illuminators.
     */
    bool containsSecondaryIlluminators() const { return m_containsSecondaryIlluminators; }

    /*! Return a bitmask with the classifications of all children
     *  in this tree.
     */
    int childClassMask() const { return m_childClassMask; }

private:
    const StarPtr starParent;
    const BodyPtr bodyParent;
    std::vector<TimelinePhasePtr> children;

    double m_boundingSphereRadius;
    double m_maxChildRadius;
    bool m_containsSecondaryIlluminators;
    bool m_changed{ true };
    int m_childClassMask;

    ReferenceFramePtr defaultFrame;
};

#endif  // _CELENGINE_FRAMETREE_H_
