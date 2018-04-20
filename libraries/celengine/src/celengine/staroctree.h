// C++ Interface: staroctree
//
// Description:
//
// Copyright (C) 2005-2009, Celestia Development Team
// Original version by Toti <root@totibox>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_STAROCTREE_H_
#define _CELENGINE_STAROCTREE_H_

#include "star.h"
#include "octree.h"

using DynamicStarOctree = DynamicOctree<Star, float>;
using DynamicStarOctreePtr = std::shared_ptr<DynamicOctree<Star, float>>;
using StarOctree = StaticOctree<Star, float>;
using StarOctreePtr = std::shared_ptr<StaticOctree<Star, float>>;
using StarHandler = OctreeProcessor<Star, float>;

#endif  // _CELENGINE_STAROCTREE_H_
