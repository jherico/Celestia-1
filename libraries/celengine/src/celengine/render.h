// render.h
//
// Copyright (C) 2001-2008, Celestia Development Team
// Contact: Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_RENDER_H_
#define _CELENGINE_RENDER_H_

#include <Eigen/Core>
#include <celmath/frustum.h>
#include "universe.h"
#include "observer.h"
#include "selection.h"
#include "starcolors.h"
#include <vector>
#include <list>
#include <string>

class RendererWatcher;
class FrameTree;
class ReferenceMark;
class CurvePlot;

struct LightSource {
    Eigen::Vector3d position;
    Color color;
    float luminosity;
    float radius;
};

struct RenderListEntry {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    enum RenderableType
    {
        RenderableStar,
        RenderableBody,
        RenderableCometTail,
        RenderableReferenceMark,
    };

    union {
        const Star* star;
        Body* body;
        const ReferenceMark* refMark;
    };

    Eigen::Vector3f position;
    Eigen::Vector3f sun;
    float distance;
    float radius;
    float centerZ;
    float nearZ;
    float farZ;
    float discSizeInPixels;
    float appMag;
    RenderableType renderableType;
    bool isOpaque;
};

struct SecondaryIlluminator {
    const Body* body;
    Eigen::Vector3d position_v;  // viewer relative position
    float radius;                // radius in km
    float reflectedIrradiance;   // albedo times total irradiance from direct sources
};

class StarVertexBuffer;
class PointStarVertexBuffer;

class Renderer {
public:
    struct DetailOptions {
        DetailOptions();
        uint32_t ringSystemSections;
        uint32_t orbitPathSamplePoints;
        uint32_t shadowTextureSize;
        uint32_t eclipseTextureSize;
        double orbitWindowEnd;
        double orbitPeriodsShown;
        double linearFadeFraction;
    };

    enum
    {
        NoLabels = 0x000,
        StarLabels = 0x001,
        PlanetLabels = 0x002,
        MoonLabels = 0x004,
        ConstellationLabels = 0x008,
        GalaxyLabels = 0x010,
        AsteroidLabels = 0x020,
        SpacecraftLabels = 0x040,
        LocationLabels = 0x080,
        CometLabels = 0x100,
        NebulaLabels = 0x200,
        OpenClusterLabels = 0x400,
        I18nConstellationLabels = 0x800,
        DwarfPlanetLabels = 0x1000,
        MinorMoonLabels = 0x2000,
        GlobularLabels = 0x4000,
        BodyLabelMask =
            (PlanetLabels | DwarfPlanetLabels | MoonLabels | MinorMoonLabels | AsteroidLabels | SpacecraftLabels | CometLabels),
    };

    enum
    {
        ShowNothing = 0x0000,
        ShowStars = 0x0001,
        ShowPlanets = 0x0002,
        ShowGalaxies = 0x0004,
        ShowDiagrams = 0x0008,
        ShowCloudMaps = 0x0010,
        ShowOrbits = 0x0020,
        ShowCelestialSphere = 0x0040,
        ShowNightMaps = 0x0080,
        ShowAtmospheres = 0x0100,
        ShowSmoothLines = 0x0200,
        ShowEclipseShadows = 0x0400,
        ShowStarsAsPoints = 0x0800,
        ShowRingShadows = 0x1000,
        ShowBoundaries = 0x2000,
        ShowAutoMag = 0x4000,
        ShowCometTails = 0x8000,
        ShowMarkers = 0x10000,
        ShowPartialTrajectories = 0x20000,
        ShowNebulae = 0x40000,
        ShowOpenClusters = 0x80000,
        ShowGlobulars = 0x100000,
        ShowCloudShadows = 0x200000,
        ShowGalacticGrid = 0x400000,
        ShowEclipticGrid = 0x800000,
        ShowHorizonGrid = 0x1000000,
        ShowEcliptic = 0x2000000,
        ShowTintedIllumination = 0x4000000,
    };

    enum StarStyle
    {
        FuzzyPointStars = 0,
        PointStars = 1,
        ScaledDiscStars = 2,
        StarStyleCount = 3,
    };

    // constants
    static const int DefaultRenderFlags = Renderer::ShowStars | Renderer::ShowPlanets | Renderer::ShowGalaxies |
                                          Renderer::ShowGlobulars | Renderer::ShowCloudMaps | Renderer::ShowAtmospheres |
                                          Renderer::ShowEclipseShadows | Renderer::ShowRingShadows | Renderer::ShowCometTails |
                                          Renderer::ShowNebulae | Renderer::ShowOpenClusters | Renderer::ShowAutoMag |
                                          Renderer::ShowSmoothLines;

    // Colors for all lines and labels
    static Color StarLabelColor;
    static Color PlanetLabelColor;
    static Color DwarfPlanetLabelColor;
    static Color MoonLabelColor;
    static Color MinorMoonLabelColor;
    static Color AsteroidLabelColor;
    static Color CometLabelColor;
    static Color SpacecraftLabelColor;
    static Color LocationLabelColor;
    static Color GalaxyLabelColor;
    static Color GlobularLabelColor;
    static Color NebulaLabelColor;
    static Color OpenClusterLabelColor;
    static Color ConstellationLabelColor;
    static Color EquatorialGridLabelColor;
    static Color PlanetographicGridLabelColor;
    static Color GalacticGridLabelColor;
    static Color EclipticGridLabelColor;
    static Color HorizonGridLabelColor;

    static Color StarOrbitColor;
    static Color PlanetOrbitColor;
    static Color DwarfPlanetOrbitColor;
    static Color MoonOrbitColor;
    static Color MinorMoonOrbitColor;
    static Color AsteroidOrbitColor;
    static Color CometOrbitColor;
    static Color SpacecraftOrbitColor;
    static Color SelectionOrbitColor;

    static Color ConstellationColor;
    static Color BoundaryColor;
    static Color EquatorialGridColor;
    static Color PlanetographicGridColor;
    static Color PlanetEquatorColor;
    static Color GalacticGridColor;
    static Color EclipticGridColor;
    static Color HorizonGridColor;
    static Color EclipticColor;

    static Color SelectionCursorColor;
};

class RendererWatcher {
public:
    RendererWatcher(){};
    virtual ~RendererWatcher(){};

    virtual void notifyRenderSettingsChanged(const Renderer*) = 0;
};

#endif  // _CELENGINE_RENDER_H_
