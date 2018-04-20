// planetgrid.cpp
//
// Longitude/latitude grids for ellipsoidal bodies.
//
// Copyright (C) 2008-2009, the Celestia Development Team
// Initial version by Chris Laurel, claurel@gmail.com
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "planetgrid.h"
#include <celmath/intersect.h>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <cmath>
#include <cstdio>

#include "body.h"

using namespace Eigen;

PlanetographicGrid::PlanetographicGrid(const BodyConstPtr& _body) : body(_body) {
    setTag("planetographic grid");
    setIAULongLatConvention();
}

PlanetographicGrid::~PlanetographicGrid() {
}

float PlanetographicGrid::boundingSphereRadius() const {
    return body->getRadius();
}

/*! Determine the longitude convention to use based on IAU rules:
 *    Westward for prograde rotators, Eastward for retrograde
 *    rotators, EastWest for the Earth and Moon.
 */
void PlanetographicGrid::setIAULongLatConvention() {
    if (body->getName() == "Earth" || body->getName() == "Moon") {
        northDirection = NorthNormal;
        longitudeConvention = EastWest;
    } else {
        if (body->getAngularVelocity(astro::J2000).y() >= 0.0) {
            northDirection = NorthNormal;
            longitudeConvention = Westward;
        } else {
            northDirection = NorthReversed;
            longitudeConvention = Eastward;
        }
    }
}
