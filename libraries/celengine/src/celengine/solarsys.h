// solarsys.h
//
// Copyright (C) 2001 Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _SOLARSYS_H_
#define _SOLARSYS_H_

#include <vector>
#include <map>
#include <iostream>
#include <Eigen/Core>

#include "forward.h"

class FrameTree;

class SolarSystem {
public:
    using Pointer = std::shared_ptr<SolarSystem>;
    SolarSystem(const StarPtr&);

    const StarPtr& getStar() const { return star; };
    Eigen::Vector3f getCenter() const;
    const PlanetarySystemPtr& getPlanets() const { return planets; };
    const FrameTreePtr& getFrameTree() const { return frameTree; };

private:
    StarPtr star;
    PlanetarySystemPtr planets;
    FrameTreePtr frameTree;
};

using SolarSystemCatalog = std::map<uint32_t, SolarSystem::Pointer>;
using SolarSystemCatalogPtr = std::shared_ptr<SolarSystemCatalog>;

class Universe;

bool LoadSolarSystemObjects(std::istream& in, Universe& universe, const std::string& dir = "");

#endif  // _SOLARSYS_H_
