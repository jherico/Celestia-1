// body.h
//
// Copyright (C) 2001-2006 Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_BODY_H_
#define _CELENGINE_BODY_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <celutil/utf8.h>
#include <celephem/rotation.h>
#include <celephem/orbit.h>

#include "surface.h"
#include "star.h"
#include "location.h"
#include "timeline.h"
#include "forward.h"

class PlanetarySystem {
public:
    PlanetarySystem(const BodyPtr& _primary);
    PlanetarySystem(const StarPtr& _star);
    ~PlanetarySystem();

    StarPtr getStar() const { return star; };
    BodyPtr getPrimaryBody() const { return primary; };
    int getSystemSize() const { return (int)satellites.size(); };
    BodyPtr getBody(int i) const { return satellites[i]; };

    void addAlias(const BodyPtr& body, const std::string& alias);
    void removeAlias(const BodyPtr& body, const std::string& alias);
    void addBody(const BodyPtr& body);
    void removeBody(const BodyPtr& body);
    void replaceBody(const BodyPtr& oldBody, const BodyPtr& newBody);

    int getOrder(const BodyPtr& body) const;

    enum TraversalResult
    {
        ContinueTraversal = 0,
        StopTraversal = 1
    };

    using TraversalFunc = std::function<bool(const BodyPtr&, void*)>;

    bool traverse(const TraversalFunc&, void*) const;
    BodyPtr find(const std::string&, bool deepSearch = false, bool i18n = false) const;
    std::vector<std::string> getCompletion(const std::string& _name, bool rec = true) const;

private:
    void addBodyToNameIndex(const BodyPtr& body);
    void removeBodyFromNameIndex(const BodyPtr& body);

private:
    typedef std::map<std::string, BodyPtr, UTF8StringOrderingPredicate> ObjectIndex;

private:
    const StarPtr star;
    const BodyPtr primary;
    std::vector<BodyPtr> satellites;
    ObjectIndex objectIndex;  // index of bodies by name
};

class RingSystem {
public:
    float innerRadius;
    float outerRadius;
    Color color;
    MultiResTexture texture;

    RingSystem(float inner, float outer) :
        innerRadius(inner), outerRadius(outer),
#ifdef HDR_COMPRESS
        color(0.5f, 0.5f, 0.5f),
#else
        color(1.0f, 1.0f, 1.0f),
#endif
        texture(){};
    RingSystem(float inner, float outer, Color _color, int _loTexture = -1, int _texture = -1) :
        innerRadius(inner), outerRadius(outer), color(_color), texture(_loTexture, _texture){};
    RingSystem(float inner, float outer, Color _color, const MultiResTexture& _texture) :
        innerRadius(inner), outerRadius(outer), color(_color), texture(_texture){};
};

class Body : public Object, public std::enable_shared_from_this<Body> {
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    Body(const PlanetarySystemPtr&, const std::string& name);
    ~Body();

    // Object class enumeration:
    // All of these values must be powers of two so that they can
    // be used in an object type bit mask.
    //
    // The values of object class enumerants cannot be modified
    // without consequence. The object orbit mask is a stored user
    // setting, so there will be unexpected results when the user
    // upgrades if the orbit mask values mean something different
    // in the new version.
    //
    // * Planet, Moon, Asteroid, DwarfPlanet, and MinorMoon all behave
    // essentially the same. They're distinguished from each other for
    // user convenience, so that it's possible to assign them different
    // orbit and label colors, and to categorize them in the solar
    // system browser.
    //
    // * Comet is identical to the asteroid class except that comets may
    // be rendered with dust and ion tails.
    //
    // Other classes have different default settings for the properties
    // Clickable, VisibleAsPoint, Visible, and SecondaryIlluminator. These
    // defaults are assigned in the ssc file parser and may be overridden
    // for a particular body.
    //
    // * Invisible is used for barycenters and other reference points.
    // An invisible object is not clickable, visibleAsPoint, visible, or
    // a secondary illuminator.
    //
    // * SurfaceFeature is meant to be used for buildings and landscape.
    // SurfaceFeatures is clickable and visible, but not visibleAsPoint or
    // a secondary illuminator.
    //
    // * Component should be used for parts of spacecraft or buildings that
    // are separate ssc objects. A component is clickable and visible, but
    // not visibleAsPoint or a secondary illuminator.
    //
    // * Diffuse is used for gas clouds, dust plumes, and the like. They are
    // visible, but other properties are false by default. It is expected
    // that an observer will move through a diffuse object, so there's no
    // need for any sort of collision detection to be applied.
    //
    // * Stellar is a pseudo-class used only for orbit rendering.
    //
    // * Barycenter and SmallBody are not used currently. Invisible is used
    // instead of barycenter.
    enum
    {
        Planet = 0x01,
        Moon = 0x02,
        Asteroid = 0x04,
        Comet = 0x08,
        Spacecraft = 0x10,
        Invisible = 0x20,
        Barycenter = 0x40,  // Not used (invisible is used instead)
        SmallBody = 0x80,   // Not used
        DwarfPlanet = 0x100,
        Stellar = 0x200,  // only used for orbit mask
        SurfaceFeature = 0x400,
        Component = 0x800,
        MinorMoon = 0x1000,
        Diffuse = 0x2000,
        Unknown = 0x10000,
    };

    enum VisibilityPolicy
    {
        NeverVisible = 0,
        UseClassVisibility = 1,
        AlwaysVisible = 2,
    };

    void setDefaultProperties();

    const PlanetarySystemPtr& getSystem() const { return system; }
    const std::vector<std::string>& getNames() const;
    std::string getName(bool i18n = false) const;
    std::string getLocalizedName() const;
    bool hasLocalizedName() const;
    void addAlias(const std::string& alias);

    void setTimeline(const TimelinePtr& timeline);
    const TimelinePtr& getTimeline() const { return timeline; }
    const FrameTreePtr& getFrameTree() const { return frameTree; }
    const FrameTreePtr& getOrCreateFrameTree();

    const ReferenceFramePtr& getOrbitFrame(double tdb) const;
    const OrbitPtr& getOrbit(double tdb) const;
    const ReferenceFramePtr& getBodyFrame(double tdb) const;
    const RotationModelPtr& getRotationModel(double tdb) const;

    // Size methods
    void setSemiAxes(const Eigen::Vector3f&);

    /*! Retrieve the body's semiaxes
    */
    Eigen::Vector3f getSemiAxes() const { return semiAxes; }
    /*! Get the radius of the body. For a spherical body, this is simply
    *  the sphere's radius. For an ellipsoidal body, the radius is the
    *  largest of the three semiaxes. For irregular bodies (with a shape
    *  represented by a mesh), the radius is the largest semiaxis of the
    *  mesh's axis aligned bounding axis. Note that this means some portions
    *  of the mesh may extend outside the sphere of the retrieved radius.
    *  To obtain the radius of a sphere that will definitely enclose the
    *  body, call getBoundingRadius() instead.
    */
    float getRadius() const { return radius; }

    bool isSphere() const;
    bool isEllipsoid() const;

    float getMass() const { return mass; }
    void setMass(float _mass) { mass = _mass; }
    float getAlbedo() const { return albedo; }
    void setAlbedo(float _albedo) { albedo = _albedo; }
    int getClassification() const { return classification; }
    void setClassification(int);
    std::string getInfoURL() const;
    void setInfoURL(const std::string&);

    const PlanetarySystemPtr& getSatellites() const { return satellites; }
    void setSatellites(const PlanetarySystemPtr& ssys) { satellites = ssys; }

    float getBoundingRadius() const;
    /*! Return the radius of sphere large enough to contain any geometry
    *  associated with this object: the primary geometry, comet tail,
    *  rings, atmosphere shell, cloud layers, or reference marks.
    */
    float getCullingRadius() const { return cullingRadius; };

    const RingSystemPtr& getRings() const { return rings; }
    void setRings(const RingSystem&);
    const AtmospherePtr& getAtmosphere() const { return atmosphere; }
    void setAtmosphere(const Atmosphere&);

    const ResourceHandle& getGeometry() const { return geometry; }
    void setGeometry(ResourceHandle _geometry) { geometry = _geometry; }
    const Eigen::Quaternionf& getGeometryOrientation() const { return geometryOrientation; }
    void setGeometryOrientation(const Eigen::Quaternionf& orientation) { geometryOrientation = orientation; }
    float getGeometryScale() const { return geometryScale; }
    /*! Set the scale factor for geometry; this is only used with unnormalized meshes.
    *  When a mesh is normalized, the effective scale factor is the radius.
    */
    void setGeometryScale(float scale) { geometryScale = scale; }

    void setSurface(const Surface& surf) { surface = surf; }
    const Surface& getSurface() const { return surface; }
    Surface& getSurface() { return surface; }

    float getLuminosity(const Star& sun, float distanceFromSun) const;
    float getLuminosity(float sunLuminosity, float distanceFromSun) const;

    float getApparentMagnitude(const Star& sun, float distanceFromSun, float distanceFromViewer) const;
    float getApparentMagnitude(float sunLuminosity, float distanceFromSun, float distanceFromViewer) const;
    float getApparentMagnitude(const Star& sun,
                               const Eigen::Vector3d& sunPosition,
                               const Eigen::Vector3d& viewerPosition) const;
    float getApparentMagnitude(float sunLuminosity,
                               const Eigen::Vector3d& sunPosition,
                               const Eigen::Vector3d& viewerPosition) const;

    UniversalCoord getPosition(double tdb) const;
    Eigen::Quaterniond getOrientation(double tdb) const;
    Eigen::Vector3d getVelocity(double tdb) const;
    Eigen::Vector3d getAngularVelocity(double tdb) const;

    Eigen::Matrix4d getLocalToAstrocentric(double) const;
    Eigen::Vector3d getAstrocentricPosition(double) const;
    Eigen::Quaterniond getEquatorialToBodyFixed(double) const;
    Eigen::Quaterniond getEclipticToFrame(double) const;
    Eigen::Quaterniond getEclipticToEquatorial(double) const;
    Eigen::Quaterniond getEclipticToBodyFixed(double) const;
    Eigen::Matrix4d getBodyFixedToAstrocentric(double) const;

    Eigen::Vector3d planetocentricToCartesian(double lon, double lat, double alt) const;
    Eigen::Vector3d planetocentricToCartesian(const Eigen::Vector3d& lonLatAlt) const;
    Eigen::Vector3d cartesianToPlanetocentric(const Eigen::Vector3d& v) const;

    Eigen::Vector3d eclipticToPlanetocentric(const Eigen::Vector3d& ecl, double tdb) const;

    bool extant(double) const;
    void setLifespan(double, double);
    void getLifespan(double&, double&) const;

    SurfacePtr getAlternateSurface(const std::string&) const;
    void addAlternateSurface(const std::string&, const SurfacePtr&);
    std::vector<std::string> getAlternateSurfaceNames() const;

    const std::vector<LocationPtr>& getLocations() const;
    void addLocation(const LocationPtr&);
    LocationPtr findLocation(const std::string&, bool i18n = false) const;
    void computeLocations();

    bool isVisible() const { return visible == 1; }
    void setVisible(bool _visible);
    bool isClickable() const { return clickable == 1; }
    void setClickable(bool _clickable);
    bool isVisibleAsPoint() const { return visibleAsPoint == 1; }
    void setVisibleAsPoint(bool _visibleAsPoint);
    bool isOrbitColorOverridden() const { return overrideOrbitColor == 1; }
    void setOrbitColorOverridden(bool override);
    bool isSecondaryIlluminator() const { return secondaryIlluminator; }
    void setSecondaryIlluminator(bool enable);

    bool hasVisibleGeometry() const { return classification != Invisible && visible != 0; }

    VisibilityPolicy getOrbitVisibility() const { return orbitVisibility; }
    void setOrbitVisibility(VisibilityPolicy _orbitVisibility);

    Color getOrbitColor() const { return orbitColor; }
    void setOrbitColor(const Color&);

    int getOrbitClassification() const;

    enum
    {
        BodyAxes = 0x01,
        FrameAxes = 0x02,
        LongLatGrid = 0x04,
        SunDirection = 0x08,
        VelocityVector = 0x10,
    };

    bool referenceMarkVisible(uint32_t) const;
    uint32_t getVisibleReferenceMarks() const;
    void setVisibleReferenceMarks(uint32_t);
    void addReferenceMark(const ReferenceMarkPtr& refMark);
    void removeReferenceMark(const std::string& tag);
    ReferenceMarkPtr findReferenceMark(const std::string& tag) const;
    const std::vector<ReferenceMarkPtr>& getReferenceMarks() const;

    StarPtr getReferenceStar() const;
    StarPtr getFrameReferenceStar() const;

    void markChanged();
    void markUpdated();

private:
    void setName(const std::string& _name);
    void recomputeCullingRadius();

private:
    std::vector<std::string> names{ 1 };
    size_t localizedNameIndex{ 0 };

    // Parent in the name hierarchy
    PlanetarySystemPtr system;
    // Children in the name hierarchy
    PlanetarySystemPtr satellites;

    TimelinePtr timeline;
    // Children in the frame hierarchy
    FrameTreePtr frameTree;

    float radius{ 1.0f };
    Eigen::Vector3f semiAxes{ 1.0f, 1.0f, 1.0f };
    float mass{ 0.0f };
    float albedo{ 0.5f };
    Eigen::Quaternionf geometryOrientation{ Eigen::Quaternionf::Identity() };

    float cullingRadius{ 0.0f };

    ResourceHandle geometry{ InvalidResource };
    float geometryScale{ 1.0f };
    Surface surface{ Color(1.0f, 1.0f, 1.0f) };

    AtmospherePtr atmosphere;
    RingSystemPtr rings;

    int classification;

    std::string infoURL;

    typedef std::map<std::string, SurfacePtr> AltSurfaceTable;
    AltSurfaceTable altSurfaces;

    std::vector<LocationPtr> locations;
    mutable bool locationsComputed{ false };

    std::vector<ReferenceMarkPtr> referenceMarks;

    Color orbitColor;

    uint32_t visible : 1;
    uint32_t clickable : 1;
    uint32_t visibleAsPoint : 1;
    uint32_t overrideOrbitColor : 1;
    VisibilityPolicy orbitVisibility : 3;
    bool secondaryIlluminator : 1;
};

#endif  // _CELENGINE_BODY_H_
