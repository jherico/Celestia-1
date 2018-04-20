// staroctree.cpp
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

#ifndef _CELENGINE_DSOOCTREE_H_
#define _CELENGINE_DSOOCTREE_H_

#include "deepskyobj.h"
#include "octree.h"

using DynamicDSOOctree = DynamicOctree<DeepSkyObject, double>;
using DSOOctree = StaticOctree<DeepSkyObject, double>;
using DSOHandler = OctreeProcessor<DeepSkyObject, double> ;

#endif  // _CELENGINE_DSOOCTREE_H_
