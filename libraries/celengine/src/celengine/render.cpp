// render.cpp
//
// Copyright (C) 2001-2009, the Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#define DEBUG_COALESCE 0
#define DEBUG_SECONDARY_ILLUMINATION 0
#define DEBUG_ORBIT_CACHE 0

//#define DEBUG_HDR
#ifdef DEBUG_HDR
//#define DEBUG_HDR_FILE
//#define DEBUG_HDR_ADAPT
//#define DEBUG_HDR_TONEMAP
#endif
#ifdef DEBUG_HDR_FILE
#include <fstream>
std::ofstream hdrlog;
#define HDR_LOG hdrlog
#else
#define HDR_LOG cout
#endif

#ifdef USE_HDR
#define BLUR_PASS_COUNT 2
#define BLUR_SIZE 128
#define DEFAULT_EXPOSURE -23.35f
#define EXPOSURE_HALFLIFE 0.4f
//#define USE_BLOOM_LISTS
#endif

// #define ENABLE_SELF_SHADOW

#ifndef _WIN32
#ifndef TARGET_OS_MAC
#include <config.h>
#endif
#endif /* _WIN32 */

#include "render.h"
#include "boundaries.h"
#include "asterism.h"
#include <celastro/astro.h>
#include "lodspheremesh.h"
#include "geometry.h"
#include "texmanager.h"
#include "meshmanager.h"
#include "renderinfo.h"
#include "frametree.h"
#include "timelinephase.h"
#include "modelgeometry.h"
#include <celutil/debug.h>
#include <celmath/frustum.h>
#include <celmath/distance.h>
#include <celmath/intersect.h>
#include <celmath/geomutil.h>
#include <celutil/utf8.h>
#include <celutil/util.h>
#include <celutil/timer.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <sstream>
#include <iomanip>
#include "eigenport.h"

using namespace cmod;
using namespace Eigen;
using namespace std;

#define FOV 45.0f
#define NEAR_DIST 0.5f
#define FAR_DIST 1.0e9f

// This should be in the GL headers, but where?
#ifndef GL_COLOR_SUM_EXT
#define GL_COLOR_SUM_EXT 0x8458
#endif

static const float STAR_DISTANCE_LIMIT = 1.0e6f;
static const int REF_DISTANCE_TO_SCREEN = 400;  //[mm]

// Contribution from planetshine beyond this distance (in units of object radius)
// is considered insignificant.
static const float PLANETSHINE_DISTANCE_LIMIT_FACTOR = 100.0f;

// Planetshine from objects less than this pixel size is treated as insignificant
// and will be ignored.
static const float PLANETSHINE_PIXEL_SIZE_LIMIT = 0.1f;

// Distance from the Sun at which comet tails will start to fade out
static const float COMET_TAIL_ATTEN_DIST_SOL = astro::AUtoKilometers(5.0f);

static const int StarVertexListSize = 1024;

// Fractional pixel offset used when rendering text as texture mapped
// quads to ensure consistent mapping of texels to pixels.
static const float PixelOffset = 0.125f;

// These two values constrain the near and far planes of the view frustum
// when rendering planet and object meshes.  The near plane will never be
// closer than MinNearPlaneDistance, and the far plane is set so that far/near
// will not exceed MaxFarNearRatio.
static const float MinNearPlaneDistance = 0.0001f;  // km
static const float MaxFarNearRatio = 2000000.0f;

static const float RenderDistance = 50.0f;

// Star disc size in pixels
static const float BaseStarDiscSize = 5.0f;
static const float MaxScaledDiscStarSize = 8.0f;
static const float GlareOpacity = 0.65f;

static const float MinRelativeOccluderRadius = 0.005f;

static const float CubeCornerToCenterDistance = (float)sqrt(3.0);

// The minimum apparent size of an objects orbit in pixels before we display
// a label for it.  This minimizes label clutter.
static const float MinOrbitSizeForLabel = 20.0f;

// The minimum apparent size of a surface feature in pixels before we display
// a label for it.
static const float MinFeatureSizeForLabel = 20.0f;

/* The maximum distance of the observer to the origin of coordinates before
   asterism lines and labels start to linearly fade out (in light years) */
static const float MaxAsterismLabelsConstDist = 6.0f;
static const float MaxAsterismLinesConstDist = 600.0f;

/* The maximum distance of the observer to the origin of coordinates before
   asterisms labels and lines fade out completely (in light years) */
static const float MaxAsterismLabelsDist = 20.0f;
static const float MaxAsterismLinesDist = 6.52e4f;

// Maximum size of a solar system in light years. Features beyond this distance
// will not necessarily be rendered correctly. This limit is used for
// visibility culling of solar systems.
static const float MaxSolarSystemSize = 1.0f;

// Static meshes and textures used by all instances of Simulation

static bool commonDataInitialized = false;

static const string EMPTY_STRING("");

LODSphereMesh* g_lodSphere = NULL;

static Texture* normalizationTex = NULL;

static Texture* starTex = NULL;
static Texture* glareTex = NULL;
static Texture* shadowTex = NULL;
static Texture* gaussianDiscTex = NULL;
static Texture* gaussianGlareTex = NULL;

// Shadow textures are scaled down slightly to leave some extra blank pixels
// near the border.  This keeps axis aligned streaks from appearing on hardware
// that doesn't support clamp to border color.
static const float ShadowTextureScale = 15.0f / 16.0f;

static Texture* eclipseShadowTextures[4];
static Texture* shadowMaskTexture = NULL;
static Texture* penumbraFunctionTexture = NULL;

Texture* rectToSphericalTexture = NULL;

static const Color compassColor(0.4f, 0.4f, 1.0f);

static const float CoronaHeight = 0.2f;

static bool buggyVertexProgramEmulation = true;

static const int MaxSkyRings = 32;
static const int MaxSkySlices = 180;
static const int MinSkySlices = 30;

// Size at which the orbit cache will be flushed of old orbit paths
static const uint32_t OrbitCacheCullThreshold = 200;
// Age in frames at which unused orbit paths may be eliminated from the cache
static const uint32_t OrbitCacheRetireAge = 16;

Color Renderer::StarLabelColor(0.471f, 0.356f, 0.682f);
Color Renderer::PlanetLabelColor(0.407f, 0.333f, 0.964f);
Color Renderer::DwarfPlanetLabelColor(0.407f, 0.333f, 0.964f);
Color Renderer::MoonLabelColor(0.231f, 0.733f, 0.792f);
Color Renderer::MinorMoonLabelColor(0.231f, 0.733f, 0.792f);
Color Renderer::AsteroidLabelColor(0.596f, 0.305f, 0.164f);
Color Renderer::CometLabelColor(0.768f, 0.607f, 0.227f);
Color Renderer::SpacecraftLabelColor(0.93f, 0.93f, 0.93f);
Color Renderer::LocationLabelColor(0.24f, 0.89f, 0.43f);
Color Renderer::GalaxyLabelColor(0.0f, 0.45f, 0.5f);
Color Renderer::GlobularLabelColor(0.8f, 0.45f, 0.5f);
Color Renderer::NebulaLabelColor(0.541f, 0.764f, 0.278f);
Color Renderer::OpenClusterLabelColor(0.239f, 0.572f, 0.396f);
Color Renderer::ConstellationLabelColor(0.225f, 0.301f, 0.36f);
Color Renderer::EquatorialGridLabelColor(0.64f, 0.72f, 0.88f);
Color Renderer::PlanetographicGridLabelColor(0.8f, 0.8f, 0.8f);
Color Renderer::GalacticGridLabelColor(0.88f, 0.72f, 0.64f);
Color Renderer::EclipticGridLabelColor(0.72f, 0.64f, 0.88f);
Color Renderer::HorizonGridLabelColor(0.72f, 0.72f, 0.72f);

Color Renderer::StarOrbitColor(0.5f, 0.5f, 0.8f);
Color Renderer::PlanetOrbitColor(0.3f, 0.323f, 0.833f);
Color Renderer::DwarfPlanetOrbitColor(0.3f, 0.323f, 0.833f);
Color Renderer::MoonOrbitColor(0.08f, 0.407f, 0.392f);
Color Renderer::MinorMoonOrbitColor(0.08f, 0.407f, 0.392f);
Color Renderer::AsteroidOrbitColor(0.58f, 0.152f, 0.08f);
Color Renderer::CometOrbitColor(0.639f, 0.487f, 0.168f);
Color Renderer::SpacecraftOrbitColor(0.4f, 0.4f, 0.4f);
Color Renderer::SelectionOrbitColor(1.0f, 0.0f, 0.0f);

Color Renderer::ConstellationColor(0.0f, 0.24f, 0.36f);
Color Renderer::BoundaryColor(0.24f, 0.10f, 0.12f);
Color Renderer::EquatorialGridColor(0.28f, 0.28f, 0.38f);
Color Renderer::PlanetographicGridColor(0.8f, 0.8f, 0.8f);
Color Renderer::PlanetEquatorColor(0.5f, 1.0f, 1.0f);
Color Renderer::GalacticGridColor(0.38f, 0.38f, 0.28f);
Color Renderer::EclipticGridColor(0.38f, 0.28f, 0.38f);
Color Renderer::HorizonGridColor(0.38f, 0.38f, 0.38f);
Color Renderer::EclipticColor(0.5f, 0.1f, 0.1f);

Color Renderer::SelectionCursorColor(1.0f, 0.0f, 0.0f);
