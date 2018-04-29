// render.cpp
//
// Copyright (C) 2001-2009, the Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "render.h"

#include <cstdio>
#include <cstring>
#include <cassert>

#include <algorithm>
#include <iomanip>
#include <sstream>

#include <celutil/debug.h>
#include <celmath/frustum.h>
#include <celmath/distance.h>
#include <celmath/intersect.h>
#include <celmath/geomutil.h>
#include <celutil/utf8.h>
#include <celutil/util.h>
#include <celutil/timer.h>

#include <celastro/astro.h>

#include "atmosphere.h"
#include "boundaries.h"
#include "asterism.h"
#include "renderinfo.h"
#include "axisarrow.h"
#include "frametree.h"
#include "timelinephase.h"
#include "eigenport.h"

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

static const string EMPTY_STRING("");

// Shadow textures are scaled down slightly to leave some extra blank pixels
// near the border.  This keeps axis aligned streaks from appearing on hardware
// that doesn't support clamp to border color.
static const float ShadowTextureScale = 15.0f / 16.0f;

static const Color compassColor(0.4f, 0.4f, 1.0f);

static const float CoronaHeight = 0.2f;

static bool buggyVertexProgramEmulation = true;

static const int MaxSkyRings = 32;
static const int MaxSkySlices = 180;
static const int MinSkySlices = 30;

// Size at which the orbit cache will be flushed of old orbit paths
static const unsigned int OrbitCacheCullThreshold = 200;
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

#ifdef ENABLE_SELF_SHADOW
static FramebufferObject* shadowFbo = NULL;
#endif

// Some useful unit conversions
inline float mmToInches(float mm) {
    return mm * (1.0f / 25.4f);
}

inline float inchesToMm(float in) {
    return in * 25.4f;
}

// Fade function for objects that shouldn't be shown when they're too small
// on screen such as orbit paths and some object labels. The will fade linearly
// from invisible at minSize pixels to full visibility at opaqueScale*minSize.
inline float sizeFade(float screenSize, float minScreenSize, float opaqueScale) {
    return min(1.0f, (screenSize - minScreenSize) / (minScreenSize * (opaqueScale - 1)));
}

// Calculate the cosine of half the maximum field of view. We'll use this for
// fast testing of object visibility.  The function takes the vertical FOV (in
// degrees) as an argument. When computing the view cone, we want the field of
// view as measured on the diagonal between viewport corners.
double computeCosViewConeAngle(double verticalFOV, double aspect) {
    double h = tan(degToRad(verticalFOV / 2));
    double diag = sqrt(1.0 + square(h) + square(h * aspect));
    return 1.0 / diag;
}

/**** Star vertex buffer classes ****/

/**** End star vertex buffer classes ****/

Renderer::Renderer() :
    corrFac(1.12f), faintestAutoMag45deg(8.0f),  //def. 7.0f
    labelMode(LocationLabels),                   //def. NoLabels
    renderFlags(ShowStars | ShowPlanets), orbitMask(Body::Planet | Body::Moon | Body::Stellar), ambientLightLevel(0.1f),
    brightnessBias(0.0f), saturationMagNight(1.0f), saturationMag(1.0f), lastOrbitCacheFlush(0),
    minOrbitSize(MinOrbitSizeForLabel), distanceLimit(1.0e6f), minFeatureSize(MinFeatureSizeForLabel), locationFilter(~0u),
    colorTemp(NULL), settingsChanged(true) {
    skyContour = new SkyContourPoint[MaxSkySlices + 1];
    colorTemp = GetStarColorTable(ColorTable_Blackbody_D65);
}

Renderer::~Renderer() {
}

static int translateLabelModeToClassMask(int labelMode) {
    int classMask = 0;

    if (labelMode & Renderer::PlanetLabels)
        classMask |= Body::Planet;
    if (labelMode & Renderer::DwarfPlanetLabels)
        classMask |= Body::DwarfPlanet;
    if (labelMode & Renderer::MoonLabels)
        classMask |= Body::Moon;
    if (labelMode & Renderer::MinorMoonLabels)
        classMask |= Body::MinorMoon;
    if (labelMode & Renderer::AsteroidLabels)
        classMask |= Body::Asteroid;
    if (labelMode & Renderer::CometLabels)
        classMask |= Body::Comet;
    if (labelMode & Renderer::SpacecraftLabels)
        classMask |= Body::Spacecraft;

    return classMask;
}

// Depth comparison function for render list entries
bool operator<(const RenderListEntry& a, const RenderListEntry& b) {
    // Operation is reversed because -z axis points into the screen
    return a.centerZ - a.radius > b.centerZ - b.radius;
}

// Depth comparison for labels
// Note that it's essential to declare this operator as a member
// function of Renderer::Label; if it's not a class member, C++'s
// argument dependent lookup will not find the operator when it's
// used as a predicate for STL algorithms.
bool Renderer::Annotation::operator<(const Annotation& a) const {
    // Operation is reversed because -z axis points into the screen
    return position.z() > a.position.z();
}

// Depth comparison for orbit paths
bool Renderer::OrbitPathListEntry::operator<(const Renderer::OrbitPathListEntry& o) const {
    // Operation is reversed because -z axis points into the screen
    return centerZ - radius > o.centerZ - o.radius;
}

void Renderer::setFaintestAM45deg(float _faintestAutoMag45deg) {
    faintestAutoMag45deg = _faintestAutoMag45deg;
    markSettingsChanged();
}

float Renderer::getFaintestAM45deg() const {
    return faintestAutoMag45deg;
}

void Renderer::setRenderMode(int _renderMode) {
    renderMode = _renderMode;
    markSettingsChanged();
}

int Renderer::getRenderFlags() const {
    return renderFlags;
}

void Renderer::setRenderFlags(int _renderFlags) {
    renderFlags = _renderFlags;
    markSettingsChanged();
}

int Renderer::getLabelMode() const {
    return labelMode;
}

void Renderer::setLabelMode(int _labelMode) {
    labelMode = _labelMode;
    markSettingsChanged();
}

int Renderer::getOrbitMask() const {
    return orbitMask;
}

void Renderer::setOrbitMask(int mask) {
    orbitMask = mask;
    markSettingsChanged();
}

const ColorTemperatureTable* Renderer::getStarColorTable() const {
    return colorTemp;
}

void Renderer::setStarColorTable(const ColorTemperatureTable* ct) {
    colorTemp = ct;
    markSettingsChanged();
}

float Renderer::getAmbientLightLevel() const {
    return ambientLightLevel;
}

void Renderer::setAmbientLightLevel(float level) {
    ambientLightLevel = level;
    markSettingsChanged();
}

float Renderer::getMinimumFeatureSize() const {
    return minFeatureSize;
}

void Renderer::setMinimumFeatureSize(float pixels) {
    minFeatureSize = pixels;
    markSettingsChanged();
}

float Renderer::getMinimumOrbitSize() const {
    return minOrbitSize;
}

// Orbits and labels are only rendered when the orbit of the object
// occupies some minimum number of pixels on screen.
void Renderer::setMinimumOrbitSize(float pixels) {
    minOrbitSize = pixels;
    markSettingsChanged();
}

float Renderer::getDistanceLimit() const {
    return distanceLimit;
}

void Renderer::setDistanceLimit(float distanceLimit_) {
    distanceLimit = distanceLimit_;
    markSettingsChanged();
}

void Renderer::addAnnotation(vector<Annotation>& annotations,
                             const MarkerRepresentationPtr& markerRep,
                             const string& labelText,
                             Color color,
                             const Vector3f& pos,
                             LabelAlignment halign,
                             LabelVerticalAlignment valign,
                             float size) {
#if 0
    double winX, winY, winZ;
    GLint view[4] = { 0, 0, windowWidth, windowHeight };
    float depth = (float)(pos.x() * modelMatrix[2] + pos.y() * modelMatrix[6] + pos.z() * modelMatrix[10]);
    if (gluProject(pos.x(), pos.y(), pos.z(), modelMatrix, projMatrix, view, &winX, &winY, &winZ) != GL_FALSE) {
        Annotation a;

        a.labelText[0] = '\0';
        ReplaceGreekLetterAbbr(a.labelText, MaxLabelLength, labelText.c_str(), labelText.length());
        a.labelText[MaxLabelLength - 1] = '\0';

        a.markerRep = markerRep;
        a.color = color;
        a.position = Vector3f((float)winX, (float)winY, -depth);
        a.halign = halign;
        a.valign = valign;
        a.size = size;
        annotations.push_back(a);
    }
#endif
}

void Renderer::addForegroundAnnotation(const MarkerRepresentationPtr& markerRep,
                                       const string& labelText,
                                       Color color,
                                       const Vector3f& pos,
                                       LabelAlignment halign,
                                       LabelVerticalAlignment valign,
                                       float size) {
    addAnnotation(foregroundAnnotations, markerRep, labelText, color, pos, halign, valign, size);
}

void Renderer::addBackgroundAnnotation(const MarkerRepresentationPtr& markerRep,
                                       const string& labelText,
                                       Color color,
                                       const Vector3f& pos,
                                       LabelAlignment halign,
                                       LabelVerticalAlignment valign,
                                       float size) {
    addAnnotation(backgroundAnnotations, markerRep, labelText, color, pos, halign, valign, size);
}

void Renderer::addSortedAnnotation(const MarkerRepresentationPtr& markerRep,
                                   const string& labelText,
                                   Color color,
                                   const Vector3f& pos,
                                   LabelAlignment halign,
                                   LabelVerticalAlignment valign,
                                   float size) {
#if 0

    double winX, winY, winZ;
    GLint view[4] = { 0, 0, windowWidth, windowHeight };
    float depth = (float)(pos.x() * modelMatrix[2] + pos.y() * modelMatrix[6] + pos.z() * modelMatrix[10]);
    if (gluProject(pos.x(), pos.y(), pos.z(), modelMatrix, projMatrix, view, &winX, &winY, &winZ) != GL_FALSE) {
        Annotation a;

        a.labelText[0] = '\0';
        if (markerRep == NULL) {
            //l.text = ReplaceGreekLetterAbbr(_(text.c_str()));
            strncpy(a.labelText, labelText.c_str(), MaxLabelLength);
            a.labelText[MaxLabelLength - 1] = '\0';
        }
        a.markerRep = markerRep;
        a.color = color;
        a.position = Vector3f((float)winX, (float)winY, -depth);
        a.halign = halign;
        a.valign = valign;
        a.size = size;
        depthSortedAnnotations.push_back(a);
    }
#endif
}

void Renderer::clearAnnotations(vector<Annotation>& annotations) {
    annotations.clear();
}

void Renderer::clearSortedAnnotations() {
    depthSortedAnnotations.clear();
}

// Return the orientation of the camera used to render the current
// frame. Available only while rendering a frame.
Quaternionf Renderer::getCameraOrientation() const {
    return m_cameraOrientation;
}

float Renderer::getNearPlaneDistance() const {
    return depthPartitions[currentIntervalIndex].nearZ;
}

void Renderer::addObjectAnnotation(const MarkerRepresentationPtr& markerRep,
                                   const string& labelText,
                                   Color color,
                                   const Vector3f& pos) {
#if 0
        double winX, winY, winZ;
        GLint view[4] = { 0, 0, windowWidth, windowHeight };
        float depth = (float)(pos.x() * modelMatrix[2] + pos.y() * modelMatrix[6] + pos.z() * modelMatrix[10]);
        if (gluProject(pos.x(), pos.y(), pos.z(), modelMatrix, projMatrix, view, &winX, &winY, &winZ) != GL_FALSE) {
            Annotation a;

            a.labelText[0] = '\0';
            if (!labelText.empty()) {
                strncpy(a.labelText, labelText.c_str(), MaxLabelLength);
                a.labelText[MaxLabelLength - 1] = '\0';
            }
            a.markerRep = markerRep;
            a.color = color;
            a.position = Vector3f((float)winX, (float)winY, -depth);
            a.size = 0.0f;

            objectAnnotations.push_back(a);
        }
    }
#endif
}

static int orbitsRendered = 0;
static int orbitsSkipped = 0;
static int sectionsCulled = 0;

// Convert a position in the universal coordinate system to astrocentric
// coordinates, taking into account possible orbital motion of the star.
static Vector3d astrocentricPosition(const UniversalCoord& pos, const Star& star, double t) {
    return pos.offsetFromKm(star.getPosition(t));
}

void Renderer::autoMag(float& faintestMag) {
    float fov = 0;
    float fieldCorr = 2.0f * FOV / (fov + FOV);
    faintestMag = (float)(faintestAutoMag45deg * sqrt(fieldCorr));
    saturationMag = saturationMagNight * (1.0f + fieldCorr * fieldCorr);
}

// Set up the light sources for rendering a solar system.  The positions of
// all nearby stars are converted from universal to viewer-centered
// coordinates.
static void setupLightSources(const vector<StarConstPtr>& nearStars,
                              const UniversalCoord& observerPos,
                              double t,
                              vector<LightSource>& lightSources,
                              int renderFlags) {
    lightSources.clear();

    for (auto star : nearStars) {
        if (star->getVisibility()) {
            Vector3d v = star->getPosition(t).offsetFromKm(observerPos);
            LightSource ls;
            ls.position = v;
            ls.luminosity = star->getLuminosity();
            ls.radius = star->getRadius();

            if (renderFlags & Renderer::ShowTintedIllumination) {
                // If the star is sufficiently cool, change the light color
                // from white.  Though our sun appears yellow, we still make
                // it and all hotter stars emit white light, as this is the
                // 'natural' light to which our eyes are accustomed.  We also
                // assign a slight bluish tint to light from O and B type stars,
                // though these will almost never have planets for their light
                // to shine upon.
                float temp = star->getTemperature();
                if (temp > 30000.0f)
                    ls.color = Color(0.8f, 0.8f, 1.0f);
                else if (temp > 10000.0f)
                    ls.color = Color(0.9f, 0.9f, 1.0f);
                else if (temp > 5400.0f)
                    ls.color = Color(1.0f, 1.0f, 1.0f);
                else if (temp > 3900.0f)
                    ls.color = Color(1.0f, 0.9f, 0.8f);
                else if (temp > 2000.0f)
                    ls.color = Color(1.0f, 0.7f, 0.7f);
                else
                    ls.color = Color(1.0f, 0.4f, 0.4f);
            } else {
                ls.color = Color(1.0f, 1.0f, 1.0f);
            }

            lightSources.push_back(ls);
        }
    }
}

// Set up the potential secondary light sources for rendering solar system
// bodies.
static void setupSecondaryLightSources(vector<SecondaryIlluminator>& secondaryIlluminators,
                                       const vector<LightSource>& primaryIlluminators) {
    float au2 = square(astro::kilometersToAU(1.0f));

    for (vector<SecondaryIlluminator>::iterator i = secondaryIlluminators.begin(); i != secondaryIlluminators.end(); i++) {
        i->reflectedIrradiance = 0.0f;

        for (vector<LightSource>::const_iterator j = primaryIlluminators.begin(); j != primaryIlluminators.end(); j++) {
            i->reflectedIrradiance += j->luminosity / ((float)(i->position_v - j->position).squaredNorm() * au2);
        }

        i->reflectedIrradiance *= i->body->getAlbedo();
    }
}

void Renderer::preRender(const Observer& observer, const Universe& universe, float faintestMagNight, const Selection& sel) {
    // Get the observer's time
    double now = observer.getTime();
    realTime = observer.getRealTime();
    settingsChanged = false;

    // Compute the size of a pixel
    auto fov = radToDeg(PI / 2.0f);
    float viewAspectRatio = 4.0f / 3.0f;
    corrFac = (0.12f * fov / FOV * fov / FOV + 1.0f);
    cosViewConeAngle = computeCosViewConeAngle(fov, viewAspectRatio);
    invCosViewAngle = 1.0 / cosViewConeAngle;
    sinViewAngle = sqrt(1.0 - square(cosViewConeAngle));


    // Get the displayed surface texture set to use from the observer
    displayedSurface = observer.getDisplayedSurface();

    locationFilter = observer.getLocationFilter();

    // Highlight the selected object
    highlightObject = sel;

    m_cameraOrientation = observer.getOrientationf();

    // Get the view frustum used for culling in camera space.
    Frustum frustum(degToRad(fov), viewAspectRatio, MinNearPlaneDistance);

    // Get the transformed frustum, used for culling in the astrocentric coordinate
    // system.
    Frustum xfrustum(degToRad(fov), viewAspectRatio, MinNearPlaneDistance);
    xfrustum.transform(observer.getOrientationf().conjugate().toRotationMatrix());

    // Set up the camera for star rendering; the units of this phase
    // are light years.
    Vector3f observerPosLY = observer.getPosition().offsetFromLy(Vector3f::Zero());

    clearSortedAnnotations();

    // Put all solar system bodies into the render list.  Stars close and
    // large enough to have discernible surface detail are also placed in
    // renderList.
    renderList.clear();
    orbitPathList.clear();
    lightSourceList.clear();
    secondaryIlluminators.clear();

    // See if we want to use AutoMag.
    if ((renderFlags & ShowAutoMag) != 0) {
        autoMag(faintestMag);
    } else {
        faintestMag = faintestMagNight;
        saturationMag = saturationMagNight;
    }

    faintestPlanetMag = faintestMag;

    if (renderFlags & ShowPlanets) {
        nearStars.clear();
        universe.getNearStars(observer.getPosition(), 1.0f, nearStars);

        // Set up direct light sources (i.e. just stars at the moment)
        setupLightSources(nearStars, observer.getPosition(), now, lightSourceList, renderFlags);

        // Traverse the frame trees of each nearby solar system and
        // build the list of objects to be rendered.
        for (const auto& sun : nearStars) {
            auto solarSystem = universe.getSolarSystem(sun);
            if (solarSystem != NULL) {
                const auto& solarSysTree = solarSystem->getFrameTree();
                if (solarSysTree != NULL) {
                    if (solarSysTree->updateRequired()) {
                        // Tree has changed, so we must recompute bounding spheres.
                        solarSysTree->recomputeBoundingSphere();
                        solarSysTree->markUpdated();
                    }

                    // Compute the position of the observer in astrocentric coordinates
                    Vector3d astrocentricObserverPos = astrocentricPosition(observer.getPosition(), *sun, now);

                    // Build render lists for bodies and orbits paths
                    buildRenderLists(astrocentricObserverPos, xfrustum,
                                     observer.getOrientation().conjugate() * -Vector3d::UnitZ(), Vector3d::Zero(), solarSysTree,
                                     observer, now);
                    if (renderFlags & ShowOrbits) {
                        buildOrbitLists(astrocentricObserverPos, observer.getOrientation(), xfrustum, solarSysTree, now);
                    }
                }
            }

            addStarOrbitToRenderList(sun, observer, now);
        }

        if ((labelMode & (BodyLabelMask)) != 0) {
            buildLabelLists(xfrustum, now);
        }
    }

    setupSecondaryLightSources(secondaryIlluminators, lightSourceList);


    Color skyColor(0.0f, 0.0f, 0.0f);

    // Scan through the render list to see if we're inside a planetary
    // atmosphere.  If so, we need to adjust the sky color as well as the
    // limiting magnitude of stars (so stars aren't visible in the daytime
    // on planets with thick atmospheres.)
    if ((renderFlags & ShowAtmospheres) != 0) {
        for (const auto& rle : renderList) {
            if (rle.renderableType == RenderListEntry::RenderableBody && rle.body->getAtmosphere() != NULL) {
                const auto& body = rle.body;
                // Compute the density of the atmosphere, and from that
                // the amount light scattering.  It's complicated by the
                // possibility that the planet is oblate and a simple distance
                // to sphere calculation will not suffice.
                auto atmosphere = body->getAtmosphere();
                float radius = body->getRadius();
                Vector3f semiAxes = body->getSemiAxes() / radius;

                Vector3f recipSemiAxes = semiAxes.cwiseInverse();
                Vector3f eyeVec = rle.position / radius;

                // Compute the orientation of the planet before axial rotation
                Quaterniond qd = body->getEclipticToEquatorial(now);
                Quaternionf q = qd.cast<float>();
                eyeVec = q * eyeVec;

                // ellipDist is not the true distance from the surface unless
                // the planet is spherical.  The quantity that we do compute
                // is the distance to the surface along a line from the eye
                // position to the center of the ellipsoid.
                float ellipDist = (eyeVec.cwiseProduct(recipSemiAxes)).norm() - 1.0f;
                if (ellipDist < atmosphere->height / radius && atmosphere->height > 0.0f) {
                    float density = 1.0f - ellipDist / (atmosphere->height / radius);
                    if (density > 1.0f)
                        density = 1.0f;

                    Vector3f sunDir = rle.sun.normalized();
                    Vector3f normal = -rle.position.normalized();
                    float illumination = Math<float>::clamp(sunDir.dot(normal) + 0.2f);

                    float lightness = illumination * density;
                    faintestMag = faintestMag - 15.0f * lightness;
                    saturationMag = saturationMag - 15.0f * lightness;
                }
            }
        }
    }

    // Now we need to determine how to scale the brightness of stars.  The
    // brightness will be proportional to the apparent magnitude, i.e.
    // a logarithmic function of the stars apparent brightness.  This mimics
    // the response of the human eye.  We sort of fudge things here and
    // maintain a minimum range of six magnitudes between faintest visible
    // and saturation; this keeps stars from popping in or out as the sun
    // sets or rises.
    if (faintestMag - saturationMag >= 6.0f)
        brightnessScale = 1.0f / (faintestMag - saturationMag);
    else
        brightnessScale = 0.1667f;

    ambientColor = Color(ambientLightLevel, ambientLightLevel, ambientLightLevel);
}

// Helper function to compute the luminosity of a perfectly
// reflective disc with the specified radius. This is used as an upper
// bound for the apparent brightness of an object when culling
// invisible objects.
static float luminosityAtOpposition(float sunLuminosity, float distanceFromSun, float objRadius) {
    // Compute the total power of the star in Watts
    double power = astro::SOLAR_POWER * sunLuminosity;

    // Compute the irradiance at the body's distance from the star
    double irradiance = power / sphereArea(distanceFromSun * 1000);

    // Compute the total energy hitting the planet; assume an albedo of 1.0, so
    // reflected energy = incident energy.
    double incidentEnergy = irradiance * circleArea(objRadius * 1000);

    // Compute the luminosity (i.e. power relative to solar power)
    return (float)(incidentEnergy / astro::SOLAR_POWER);
}

// Compute a rough estimate of the visible length of the dust tail.
// TODO: This is old code that needs to be rewritten. For one thing,
// the length is inversely proportional to the distance from the sun,
// whereas the 1/distance^2 is probably more realistic. There should
// also be another parameter that specifies how active the comet is.
static float cometDustTailLength(float distanceToSun, float radius) {
    return (1.0e8f / distanceToSun) * (radius / 5.0f) * 1.0e7f;
}

void Renderer::addRenderListEntries(RenderListEntry& rle, const BodyPtr& bodyPtr, bool isLabeled) {
    const auto& body = *bodyPtr;
    bool visibleAsPoint = rle.appMag < faintestPlanetMag && body.isVisibleAsPoint();

    if (rle.discSizeInPixels > 1 || visibleAsPoint || isLabeled) {
        rle.renderableType = RenderListEntry::RenderableBody;
        rle.body = bodyPtr;
        rle.isOpaque = true;
        rle.radius = body.getRadius();
        renderList.push_back(rle);
    }

    if (body.getClassification() == Body::Comet && (renderFlags & ShowCometTails) != 0) {
        float radius = cometDustTailLength(rle.sun.norm(), body.getRadius());
        float discSize = (radius / (float)rle.distance) / pixelSize;
        if (discSize > 1) {
            rle.renderableType = RenderListEntry::RenderableCometTail;
            rle.body = bodyPtr;
            rle.isOpaque = false;
            rle.radius = radius;
            rle.discSizeInPixels = discSize;
            renderList.push_back(rle);
        }
    }

    for (const auto& rm : body.getReferenceMarks()) {
        rle.renderableType = RenderListEntry::RenderableReferenceMark;
        rle.refMark = rm;
        rle.isOpaque = rm->isOpaque();
        rle.radius = rm->boundingSphereRadius();
        renderList.push_back(rle);
    }
}

void Renderer::buildRenderLists(const Vector3d& astrocentricObserverPos,
                                const Frustum& viewFrustum,
                                const Vector3d& viewPlaneNormal,
                                const Vector3d& frameCenter,
                                const FrameTreePtr& tree,
                                const Observer& observer,
                                double now) {

    int labelClassMask = translateLabelModeToClassMask(labelMode);
    Matrix3f viewMat = observer.getOrientationf().toRotationMatrix();
    Vector3f viewMatZ = viewMat.row(2);

    auto nChildren = tree ? tree->childCount() : 0;
    for (unsigned int i = 0; i < nChildren; i++) {
        auto phase = tree->getChild(i);

        // No need to do anything if the phase isn't active now
        if (!phase->includes(now))
            continue;

        auto body = phase->body();

        // pos_s: sun-relative position of object
        // pos_v: viewer-relative position of object

        // Get the position of the body relative to the sun.
        Vector3d p = phase->orbit()->positionAtTime(now);
        const auto& frame = phase->orbitFrame();
        Vector3d pos_s = frameCenter + frame->getOrientation(now).conjugate() * p;

        // We now have the positions of the observer and the planet relative
        // to the sun.  From these, compute the position of the body
        // relative to the observer.
        Vector3d pos_v = pos_s - astrocentricObserverPos;

        // dist_vn: distance along view normal from the viewer to the
        // projection of the object's center.
        double dist_vn = viewPlaneNormal.dot(pos_v);

        // Vector from object center to its projection on the view normal.
        Vector3d toViewNormal = pos_v - dist_vn * viewPlaneNormal;

        float cullingRadius = body->getCullingRadius();

        // The result of the planetshine test can be reused for the view cone
        // test, but only when the object's light influence sphere is larger
        // than the geometry. This is not
        bool viewConeTestFailed = false;
        if (body->isSecondaryIlluminator()) {
            float influenceRadius = body->getBoundingRadius() + (body->getRadius() * PLANETSHINE_DISTANCE_LIMIT_FACTOR);
            if (dist_vn > -influenceRadius) {
                double maxPerpDist = (influenceRadius + dist_vn * sinViewAngle) * invCosViewAngle;
                double perpDistSq = toViewNormal.squaredNorm();
                if (perpDistSq < maxPerpDist * maxPerpDist) {
                    if ((body->getRadius() / (float)pos_v.norm()) / pixelSize > PLANETSHINE_PIXEL_SIZE_LIMIT) {
                        // add to planetshine list if larger than 1/10 pixel
                        SecondaryIlluminator illum;
                        illum.body = body;
                        illum.position_v = pos_v;
                        illum.radius = body->getRadius();
                        secondaryIlluminators.push_back(illum);
                    }
                } else {
                    viewConeTestFailed = influenceRadius > cullingRadius;
                }
            } else {
                viewConeTestFailed = influenceRadius > cullingRadius;
            }
        }

        bool insideViewCone = false;
        if (!viewConeTestFailed) {
            float radius = body->getCullingRadius();
            if (dist_vn > -radius) {
                double maxPerpDist = (radius + dist_vn * sinViewAngle) * invCosViewAngle;
                double perpDistSq = toViewNormal.squaredNorm();
                insideViewCone = perpDistSq < maxPerpDist * maxPerpDist;
            }
        }

        if (insideViewCone) {
            // Calculate the distance to the viewer
            double dist_v = pos_v.norm();

            // Calculate the size of the planet/moon disc in pixels
            float discSize = (body->getCullingRadius() / (float)dist_v) / pixelSize;

            // Compute the apparent magnitude; instead of summing the reflected
            // light from all nearby stars, we just consider the one with the
            // highest apparent brightness.
            float appMag = 100.0f;
            for (unsigned int li = 0; li < lightSourceList.size(); li++) {
                Vector3d sunPos = pos_v - lightSourceList[li].position;
                appMag = min(appMag, body->getApparentMagnitude(lightSourceList[li].luminosity, sunPos, pos_v));
            }

            bool visibleAsPoint = appMag < faintestPlanetMag && body->isVisibleAsPoint();
            bool isLabeled = (body->getOrbitClassification() & labelClassMask) != 0;
            bool visible = body->isVisible();

            if ((discSize > 1 || visibleAsPoint || isLabeled) && visible) {
                RenderListEntry rle;

                rle.position = pos_v.cast<float>();
                rle.distance = (float)dist_v;
                rle.centerZ = pos_v.cast<float>().dot(viewMatZ);
                rle.appMag = appMag;
                rle.discSizeInPixels = body->getRadius() / ((float)dist_v * pixelSize);

                // TODO: Remove this. It's only used in two places: for calculating comet tail
                // length, and for calculating sky brightness to adjust the limiting magnitude.
                // In both cases, it's the wrong quantity to use (e.g. for objects with orbits
                // defined relative to the SSB.)
                rle.sun = -pos_s.cast<float>();

                addRenderListEntries(rle, body, isLabeled);
            }
        }

        const auto& subtree = body->getFrameTree();
        if (subtree) {
            double dist_v = pos_v.norm();
            bool traverseSubtree = false;

            // There are two different tests available to determine whether we can reject
            // the object's subtree. If the subtree contains no light reflecting objects,
            // then render the subtree only when:
            //    - the subtree bounding sphere intersects the view frustum, and
            //    - the subtree contains an object bright or large enough to be visible.
            // Otherwise, render the subtree when any of the above conditions are
            // true or when a subtree object could potentially illuminate something
            // in the view cone.
            float minPossibleDistance = (float)(dist_v - subtree->boundingSphereRadius());
            float brightestPossible = 0.0;
            float largestPossible = 0.0;

            // If the viewer is not within the subtree bounding sphere, see if we can cull it because
            // it contains no objects brighter than the limiting magnitude and no objects that will
            // be larger than one pixel in size.
            if (minPossibleDistance > 1.0f) {
                // Figure out the magnitude of the brightest possible object in the subtree.

                // Compute the luminosity from reflected light of the largest object in the subtree
                float lum = 0.0f;
                for (unsigned int li = 0; li < lightSourceList.size(); li++) {
                    Vector3d sunPos = pos_v - lightSourceList[li].position;
                    lum += luminosityAtOpposition(lightSourceList[li].luminosity, (float)sunPos.norm(),
                                                  (float)subtree->maxChildRadius());
                }
                brightestPossible = astro::lumToAppMag(lum, astro::kilometersToLightYears(minPossibleDistance));
                largestPossible = (float)subtree->maxChildRadius() / (float)minPossibleDistance / pixelSize;
            } else {
                // Viewer is within the bounding sphere, so the object could be very close.
                // Assume that an object in the subree could be very bright or large,
                // so no culling will occur.
                brightestPossible = -100.0f;
                largestPossible = 100.0f;
            }

            if (brightestPossible < faintestPlanetMag || largestPossible > 1.0f) {
                // See if the object or any of its children are within the view frustum
                if (viewFrustum.testSphere(pos_v.cast<float>(), (float)subtree->boundingSphereRadius()) != Frustum::Outside) {
                    traverseSubtree = true;
                }
            }

            // If the subtree contains secondary illuminators, do one last check if it hasn't
            // already been determined if we need to traverse the subtree: see if something
            // in the subtree could possibly contribute significant illumination to an
            // object in the view cone.
            if (subtree->containsSecondaryIlluminators() && !traverseSubtree &&
                largestPossible > PLANETSHINE_PIXEL_SIZE_LIMIT) {
                float influenceRadius =
                    (float)(subtree->boundingSphereRadius() + (subtree->maxChildRadius() * PLANETSHINE_DISTANCE_LIMIT_FACTOR));
                if (dist_vn > -influenceRadius) {
                    double maxPerpDist = (influenceRadius + dist_vn * sinViewAngle) * invCosViewAngle;
                    double perpDistSq = toViewNormal.squaredNorm();
                    if (perpDistSq < maxPerpDist * maxPerpDist)
                        traverseSubtree = true;
                }
            }

            if (traverseSubtree) {
                buildRenderLists(astrocentricObserverPos, viewFrustum, viewPlaneNormal, pos_s, subtree, observer, now);
            }
        }  // end subtree traverse
    }
}

void Renderer::buildOrbitLists(const Vector3d& astrocentricObserverPos,
                               const Quaterniond& observerOrientation,
                               const Frustum& viewFrustum,
                               const FrameTreePtr& tree,
                               double now) {
    Matrix3d viewMat = observerOrientation.toRotationMatrix();
    Vector3d viewMatZ = viewMat.row(2);

    auto nChildren = tree != NULL ? tree->childCount() : 0;
    for (unsigned int i = 0; i < nChildren; i++) {
        auto phase = tree->getChild(i);

        // No need to do anything if the phase isn't active now
        if (!phase->includes(now))
            continue;

        auto body = phase->body();

        // pos_s: sun-relative position of object
        // pos_v: viewer-relative position of object

        // Get the position of the body relative to the sun.
        Vector3d pos_s = body->getAstrocentricPosition(now);

        // We now have the positions of the observer and the planet relative
        // to the sun.  From these, compute the position of the body
        // relative to the observer.
        Vector3d pos_v = pos_s - astrocentricObserverPos;

        // Only show orbits for major bodies or selected objects.
        Body::VisibilityPolicy orbitVis = body->getOrbitVisibility();

        if (body->isVisible() &&
            (body == highlightObject.body() || orbitVis == Body::AlwaysVisible ||
             (orbitVis == Body::UseClassVisibility && (body->getOrbitClassification() & orbitMask) != 0))) {
            Vector3d orbitOrigin = Vector3d::Zero();
            Selection centerObject = phase->orbitFrame()->getCenter();
            if (centerObject.body() != NULL) {
                orbitOrigin = centerObject.body()->getAstrocentricPosition(now);
            }

            // Calculate the origin of the orbit relative to the observer
            Vector3d relOrigin = orbitOrigin - astrocentricObserverPos;

            // Compute the size of the orbit in pixels
            double originDistance = pos_v.norm();
            double boundingRadius = body->getOrbit(now)->getBoundingRadius();
            float orbitRadiusInPixels = (float)(boundingRadius / (originDistance * pixelSize));

            if (orbitRadiusInPixels > minOrbitSize) {
                // Add the orbit of this body to the list of orbits to be rendered
                OrbitPathListEntry path;
                path.body = body;
                path.star = NULL;
                path.centerZ = (float)relOrigin.dot(viewMatZ);
                path.radius = (float)boundingRadius;
                path.origin = relOrigin;
                path.opacity = sizeFade(orbitRadiusInPixels, minOrbitSize, 2.0f);
                orbitPathList.push_back(path);
            }
        }

        auto subtree = body->getFrameTree();
        if (subtree != NULL) {
            // Only try to render orbits of child objects when:
            //   - The apparent size of the subtree bounding sphere is large enough that
            //     orbit paths will be visible, and
            //   - The subtree bounding sphere isn't outside the view frustum
            double dist_v = pos_v.norm();
            float distanceToBoundingSphere = (float)(dist_v - subtree->boundingSphereRadius());
            bool traverseSubtree = false;
            if (distanceToBoundingSphere > 0.0f) {
                // We're inside the subtree's bounding sphere
                traverseSubtree = true;
            } else {
                float maxPossibleOrbitSize = (float)subtree->boundingSphereRadius() / ((float)dist_v * pixelSize);
                if (maxPossibleOrbitSize > minOrbitSize)
                    traverseSubtree = true;
            }

            if (traverseSubtree) {
                // See if the object or any of its children are within the view frustum
                if (viewFrustum.testSphere(pos_v.cast<float>(), (float)subtree->boundingSphereRadius()) != Frustum::Outside) {
                    buildOrbitLists(astrocentricObserverPos, observerOrientation, viewFrustum, subtree, now);
                }
            }
        }  // end subtree traverse
    }
}

void Renderer::buildLabelLists(const Frustum& viewFrustum, double now) {
    int labelClassMask = translateLabelModeToClassMask(labelMode);
    BodyConstPtr lastPrimary;
    Sphered primarySphere;

    for (const auto& rle : renderList) {
        auto iter = &rle;
        int classification = iter->body->getOrbitClassification();
        if (iter->renderableType == RenderListEntry::RenderableBody && (classification & labelClassMask) &&
            viewFrustum.testSphere(iter->position, iter->radius) != Frustum::Outside) {
            auto body = iter->body;
            Vector3f pos = iter->position;

            float boundingRadiusSize = (float)(body->getOrbit(now)->getBoundingRadius() / iter->distance) / pixelSize;
            if (boundingRadiusSize > minOrbitSize) {
                Color labelColor;
                float opacity = sizeFade(boundingRadiusSize, minOrbitSize, 2.0f);

                switch (classification) {
                    case Body::Planet:
                        labelColor = PlanetLabelColor;
                        break;
                    case Body::DwarfPlanet:
                        labelColor = DwarfPlanetLabelColor;
                        break;
                    case Body::Moon:
                        labelColor = MoonLabelColor;
                        break;
                    case Body::MinorMoon:
                        labelColor = MinorMoonLabelColor;
                        break;
                    case Body::Asteroid:
                        labelColor = AsteroidLabelColor;
                        break;
                    case Body::Comet:
                        labelColor = CometLabelColor;
                        break;
                    case Body::Spacecraft:
                        labelColor = SpacecraftLabelColor;
                        break;
                    default:
                        labelColor = Color::Black;
                        break;
                }

                labelColor = Color(labelColor, opacity * labelColor.alpha());

                if (!body->getName().empty()) {
                    bool isBehindPrimary = false;

                    auto phase = body->getTimeline()->findPhase(now);
                    auto primary = phase->orbitFrame()->getCenter().body();
                    if (primary != NULL && (primary->getClassification() & Body::Invisible) != 0) {
                        auto parent = phase->orbitFrame()->getCenter().body();
                        if (parent != NULL)
                            primary = parent;
                    }

                    // Position the label slightly in front of the object along a line from
                    // object center to viewer.
                    pos = pos * (1.0f - body->getBoundingRadius() * 1.01f / pos.norm());

                    // Try and position the label so that it's not partially
                    // occluded by other objects. We'll consider just the object
                    // that the labeled body is orbiting (its primary) as a
                    // potential occluder. If a ray from the viewer to labeled
                    // object center intersects the occluder first, skip
                    // rendering the object label. Otherwise, ensure that the
                    // label is completely in front of the primary by projecting
                    // it onto the plane tangent to the primary at the
                    // viewer-primary intersection point. Whew. Don't do any of
                    // this if the primary isn't an ellipsoid.
                    //
                    // This only handles the problem of partial label occlusion
                    // for low orbiting and surface positioned objects, but that
                    // case is *much* more common than other possibilities.
                    if (primary != NULL && primary->isEllipsoid()) {
                        // In the typical case, we're rendering labels for many
                        // objects that orbit the same primary. Avoid repeatedly
                        // calling getPosition() by caching the last primary
                        // position.
                        if (primary != lastPrimary) {
                            Vector3d p =
                                phase->orbitFrame()->getOrientation(now).conjugate() * phase->orbit()->positionAtTime(now);
                            Vector3d v = iter->position.cast<double>() - p;

                            primarySphere = Sphered(v, primary->getRadius());
                            lastPrimary = primary;
                        }

                        Ray3d testRay(Vector3d::Zero(), pos.cast<double>());

                        // Test the viewer-to-labeled object ray against
                        // the primary sphere (TODO: handle ellipsoids)
                        double t = 0.0;
                        if (testIntersection(testRay, primarySphere, t)) {
                            // Center of labeled object is behind primary
                            // sphere; mark it for rejection.
                            isBehindPrimary = t < 1.0;
                        }

                        if (!isBehindPrimary) {
                            // Not rejected. Compute the plane tangent to
                            // the primary at the viewer-to-primary
                            // intersection point.
                            Vec3d primaryVec(primarySphere.center.x(), primarySphere.center.y(), primarySphere.center.z());
                            double distToPrimary = primaryVec.length();
                            Planed primaryTangentPlane(primaryVec, primaryVec * (primaryVec *
                                                                                 (1.0 - primarySphere.radius / distToPrimary)));

                            // Compute the intersection of the viewer-to-labeled
                            // object ray with the tangent plane.
                            float u = (float)(primaryTangentPlane.d /
                                              (primaryTangentPlane.normal * Vec3d(pos.x(), pos.y(), pos.z())));

                            // If the intersection point is closer to the viewer
                            // than the label, then project the label onto the
                            // tangent plane.
                            if (u < 1.0f && u > 0.0f) {
                                pos = pos * u;
                            }
                        }
                    }

                    addSortedAnnotation(NULL, body->getName(true), labelColor, pos);
                }
            }
        }
    }  // for each render list entry
}

// Add a star orbit to the render list
void Renderer::addStarOrbitToRenderList(const StarConstPtr& star, const Observer& observer, double now) {
    // If the star isn't fixed, add its orbit to the render list
    if ((renderFlags & ShowOrbits) != 0 && ((orbitMask & Body::Stellar) != 0 || highlightObject.star() == star)) {
        Matrix3d viewMat = observer.getOrientation().toRotationMatrix();
        Vector3d viewMatZ = viewMat.row(2);

        if (star->getOrbit() != NULL) {
            // Get orbit origin relative to the observer
            Vector3d orbitOrigin = star->getOrbitBarycenterPosition(now).offsetFromKm(observer.getPosition());

            // Compute the size of the orbit in pixels
            double originDistance = orbitOrigin.norm();
            double boundingRadius = star->getOrbit()->getBoundingRadius();
            float orbitRadiusInPixels = (float)(boundingRadius / (originDistance * pixelSize));

            if (orbitRadiusInPixels > minOrbitSize) {
                // Add the orbit of this body to the list of orbits to be rendered
                OrbitPathListEntry path;
                path.star = star;
                path.body = NULL;
                path.centerZ = (float)orbitOrigin.dot(viewMatZ);
                path.radius = (float)boundingRadius;
                path.origin = orbitOrigin;
                path.opacity = sizeFade(orbitRadiusInPixels, minOrbitSize, 2.0f);
                orbitPathList.push_back(path);
            }
        }
    }
}

void Renderer::notifyWatchers() const {
    for (const auto& watcher : watchers) {
        watcher->notifyRenderSettingsChanged(this);
    }
}
