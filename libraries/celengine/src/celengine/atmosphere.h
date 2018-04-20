// atmosphere.h
//
// Copyright (C) 2001-2009, the Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_ATMOSPHERE_H_
#define _CELENGINE_ATMOSPHERE_H_

#include <celutil/reshandle.h>
#include <celutil/color.h>
#include <Eigen/Core>

#include "multitexture.h"

class Atmosphere {
public:
    float height{ 0 };
    Color lowerColor;
    Color upperColor;
    Color skyColor;
    Color sunsetColor{ 1.0f, 0.6f, 0.5f };

    float cloudHeight{ 0.0f };
    float cloudSpeed{ 0.0f };
    MultiResTexture cloudTexture;
    MultiResTexture cloudNormalMap;

    float mieCoeff{ 0 };
    float mieScaleHeight{ 0 };
    float miePhaseAsymmetry{ 0 };
    Eigen::Vector3f rayleighCoeff{ Eigen::Vector3f::Zero() };
    float rayleighScaleHeight{ 0 };
    Eigen::Vector3f absorptionCoeff{ Eigen::Vector3f::Zero() };

    float cloudShadowDepth{ 0 };
};

// Atmosphere density is modeled with a exp(-y/H) falloff, where
// H is the scale height of the atmosphere. Thus atmospheres have
// infinite extent, but we still need to choose some finite sphere
// to render. The radius of the sphere is the height at which the
// density of the atmosphere falls to the extinction threshold, i.e.
// -H * ln(extinctionThreshold)
extern const double AtmosphereExtinctionThreshold;

#endif  // _CELENGINE_ATMOSPHERE_H_
