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

#include <vector>
#include <list>
#include <string>

#include <Eigen/Core>

#include <celmath/frustum.h>

#include "universe.h"
#include "observer.h"
#include "selection.h"
#include "starcolors.h"
#include "lightenv.h"

class RendererWatcher;
class FrameTree;
class ReferenceMark;

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

    StarConstPtr star;
    BodyConstPtr body;
    ReferenceMarkConstPtr refMark;

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
    BodyConstPtr body;
    Eigen::Vector3d position_v;  // viewer relative position
    float radius;                // radius in km
    float reflectedIrradiance;   // albedo times total irradiance from direct sources
};

class Renderer {
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    Renderer();
    ~Renderer();

    virtual void initialize(){};
    virtual void shutdown(){};

    void setFaintestAM45deg(float);
    float getFaintestAM45deg() const;
    void setRenderMode(int);
    void autoMag(float& faintestMag);

    void preRender(const Observer&, const Universe&, float faintestVisible, const Selection& sel);

    virtual void render(const ObserverPtr&, const UniversePtr&, float faintestVisible, const Selection& sel) = 0;
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
                                          Renderer::ShowNebulae | Renderer::ShowOpenClusters | Renderer::ShowAutoMag;

    int getRenderFlags() const;
    void setRenderFlags(int);
    int getLabelMode() const;
    void setLabelMode(int);
    float getAmbientLightLevel() const;
    void setAmbientLightLevel(float);
    float getMinimumOrbitSize() const;
    void setMinimumOrbitSize(float);
    float getMinimumFeatureSize() const;
    void setMinimumFeatureSize(float);
    float getDistanceLimit() const;
    void setDistanceLimit(float);
    int getOrbitMask() const;
    void setOrbitMask(int);
    const ColorTemperatureTable* getStarColorTable() const;
    void setStarColorTable(const ColorTemperatureTable*);

    // Label related methods
    enum LabelAlignment
    {
        AlignCenter,
        AlignLeft,
        AlignRight
    };

    enum LabelVerticalAlignment
    {
        VerticalAlignCenter,
        VerticalAlignBottom,
        VerticalAlignTop,
    };

    static const int MaxLabelLength = 48;
    struct Annotation {
        char labelText[MaxLabelLength];
        const MarkerRepresentation* markerRep;
        Color color;
        Eigen::Vector3f position;
        LabelAlignment halign : 3;
        LabelVerticalAlignment valign : 3;
        float size;

        bool operator<(const Annotation&) const;
    };

    void addForegroundAnnotation(const MarkerRepresentationPtr& markerRep,
                                 const std::string& labelText,
                                 Color color,
                                 const Eigen::Vector3f& position,
                                 LabelAlignment halign = AlignLeft,
                                 LabelVerticalAlignment valign = VerticalAlignBottom,
                                 float size = 0.0f);
    void addBackgroundAnnotation(const MarkerRepresentationPtr& markerRep,
                                 const std::string& labelText,
                                 Color color,
                                 const Eigen::Vector3f& position,
                                 LabelAlignment halign = AlignLeft,
                                 LabelVerticalAlignment valign = VerticalAlignBottom,
                                 float size = 0.0f);
    void addSortedAnnotation(const MarkerRepresentationPtr& markerRep,
                             const std::string& labelText,
                             Color color,
                             const Eigen::Vector3f& position,
                             LabelAlignment halign = AlignLeft,
                             LabelVerticalAlignment valign = VerticalAlignBottom,
                             float size = 0.0f);

    // Callbacks for renderables; these belong in a special renderer interface
    // only visible in object's render methods.
    void addObjectAnnotation(const MarkerRepresentationPtr& markerRep,
                             const std::string& labelText,
                             Color,
                             const Eigen::Vector3f&);
    Eigen::Quaternionf getCameraOrientation() const;
    float getNearPlaneDistance() const;

    void clearAnnotations(std::vector<Annotation>&);
    void clearSortedAnnotations();

    struct OrbitPathListEntry {
        float centerZ;
        float radius;
        BodyPtr body;
        StarConstPtr star;
        Eigen::Vector3d origin;
        float opacity;

        bool operator<(const OrbitPathListEntry&) const;
    };

    enum FontStyle
    {
        FontNormal = 0,
        FontLarge = 1,
        FontCount = 2,
    };

    bool settingsHaveChanged() const { return settingsChanged; }
    void markSettingsChanged() {
        settingsChanged = true;
        notifyWatchers();
    }

    void addWatcher(RendererWatcher* watcher) { watchers.insert(watchers.end(), watcher); }
    void removeWatcher(RendererWatcher* watcher) { std::remove(watchers.begin(), watchers.end(), watcher); }
    void notifyWatchers() const;

public:
    // Internal types
    // TODO: Figure out how to make these private.  Even with a friend
    //
    struct Particle {
        Eigen::Vector3f center;
        float size;
        Color color;
        float pad0, pad1, pad2;
    };

    struct RenderProperties {
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW

        RenderProperties() :
            surface(NULL), atmosphere(NULL), rings(NULL), radius(1.0f), geometryScale(1.0f), semiAxes(1.0f, 1.0f, 1.0f),
            geometry(InvalidResource), orientation(Eigen::Quaternionf::Identity()){};

        SurfacePtr surface;
        const AtmospherePtr atmosphere;
        RingSystemPtr rings;
        float radius;
        float geometryScale;
        Eigen::Vector3f semiAxes;
        ResourceHandle geometry;
        Eigen::Quaternionf orientation;
        LightingState::EclipseShadowVector* eclipseShadows;
    };

private:
    struct SkyVertex {
        float x, y, z;
        unsigned char color[4];
    };

    struct SkyContourPoint {
        Eigen::Vector3f v;
        Eigen::Vector3f eyeDir;
        float centerDist;
        float eyeDist;
        float cosSkyCapAltitude;
    };

    template <class OBJ>
    struct ObjectLabel {
        OBJ* obj;
        std::string label;

        ObjectLabel() : obj(NULL), label(""){};

        ObjectLabel(OBJ* _obj, const std::string& _label) : obj(_obj), label(_label){};

        ObjectLabel(const ObjectLabel& objLbl) : obj(objLbl.obj), label(objLbl.label){};

        ObjectLabel& operator=(const ObjectLabel& objLbl) {
            obj = objLbl.obj;
            label = objLbl.label;
            return *this;
        };
    };

    typedef ObjectLabel<Star> StarLabel;
    typedef ObjectLabel<DeepSkyObject> DSOLabel;  // currently not used

    struct DepthBufferPartition {
        int index;
        float nearZ;
        float farZ;
    };

private:
    void buildRenderLists(const Eigen::Vector3d& astrocentricObserverPos,
                          const Frustum& viewFrustum,
                          const Eigen::Vector3d& viewPlaneNormal,
                          const Eigen::Vector3d& frameCenter,
                          const FrameTreePtr& tree,
                          const Observer& observer,
                          double now);
    void buildOrbitLists(const Eigen::Vector3d& astrocentricObserverPos,
                         const Eigen::Quaterniond& observerOrientation,
                         const Frustum& viewFrustum,
                         const FrameTreePtr& tree,
                         double now);
    void buildLabelLists(const Frustum& viewFrustum, double now);

    void addRenderListEntries(RenderListEntry& rle, const BodyPtr& body, bool isLabeled);

    void addStarOrbitToRenderList(const StarConstPtr& star, const Observer& observer, double now);

    void addAnnotation(std::vector<Annotation>&,
                       const MarkerRepresentationPtr&,
                       const std::string& labelText,
                       Color color,
                       const Eigen::Vector3f& position,
                       LabelAlignment halign = AlignLeft,
                       LabelVerticalAlignment = VerticalAlignBottom,
                       float size = 0.0f);

protected:
    float corrFac;
    float pixelSize;
    float faintestAutoMag45deg;

    int renderMode;
    int labelMode;
    int renderFlags;
    int orbitMask;
    float ambientLightLevel;
    float brightnessBias;

    float brightnessScale;
    float faintestMag;
    float faintestPlanetMag;
    float saturationMagNight;
    float saturationMag;

    Color ambientColor;
    std::string displayedSurface;

    Eigen::Quaternionf m_cameraOrientation;
    std::vector<RenderListEntry> renderList;
    std::vector<SecondaryIlluminator> secondaryIlluminators;
    std::vector<DepthBufferPartition> depthPartitions;
    std::vector<Particle> glareParticles;
    std::vector<Annotation> backgroundAnnotations;
    std::vector<Annotation> foregroundAnnotations;
    std::vector<Annotation> depthSortedAnnotations;
    std::vector<Annotation> objectAnnotations;
    std::vector<OrbitPathListEntry> orbitPathList;
    LightingState::EclipseShadowVector eclipseShadows[MaxLights];
    std::vector<StarConstPtr> nearStars;
    std::vector<LightSource> lightSourceList;
    int currentIntervalIndex;

private:
    uint32_t lastOrbitCacheFlush;

    float minOrbitSize;
    float distanceLimit;
    float minFeatureSize;
    uint32_t locationFilter;
    SkyContourPoint* skyContour;
    const ColorTemperatureTable* colorTemp;
    Selection highlightObject;
    bool settingsChanged;
    double realTime;

    double cosViewConeAngle;
    double invCosViewAngle;
    double sinViewAngle;

    // Location markers
public:
    MarkerRepresentation mountainRep;
    MarkerRepresentation craterRep;
    MarkerRepresentation observatoryRep;
    MarkerRepresentation cityRep;
    MarkerRepresentation genericLocationRep;
    MarkerRepresentation galaxyRep;
    MarkerRepresentation nebulaRep;
    MarkerRepresentation openClusterRep;
    MarkerRepresentation globularRep;

    std::list<RendererWatcher*> watchers;

public:
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
