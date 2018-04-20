#pragma once

#include <memory>

class Object {
};

using ObjectPtr = std::shared_ptr<Object>;

class Atmosphere;
using AtmospherePtr = std::shared_ptr<Atmosphere>;
class Body;
using BodyPtr = std::shared_ptr<Body>;
using BodyConstPtr = std::shared_ptr<const Body>;
class ConstellationBoundaries;
using ConstellationBoundariesPtr = std::shared_ptr<ConstellationBoundaries>;
class DeepSkyObject;
using DeepSkyObjectPtr = std::shared_ptr<DeepSkyObject>;
class DSODatabase;
using DSODatabasePtr = std::shared_ptr<DSODatabase>;
class FrameTree;
using FrameTreePtr = std::shared_ptr<FrameTree>;
class Location;
using LocationPtr = std::shared_ptr<Location>;
class Observer;
using ObserverPtr = std::shared_ptr<Observer>;
class ObserverFrame;
using ObserverFramePtr = std::shared_ptr<ObserverFrame>;
class Orbit;
using OrbitPtr = std::shared_ptr<Orbit>;
class PlanetarySystem;
using PlanetarySystemPtr = std::shared_ptr<PlanetarySystem>;
class ReferenceFrame;
using ReferenceFramePtr = std::shared_ptr<ReferenceFrame>;
using ReferenceFrameConstPtr = std::shared_ptr<const ReferenceFrame>;
class RingSystem;
using RingSystemPtr = std::shared_ptr<RingSystem>;
class ReferenceMark;
using ReferenceMarkPtr = std::shared_ptr<ReferenceMark>;
class RotationModel;
using RotationModelPtr = std::shared_ptr<RotationModel>;
class Surface;
using SurfacePtr = std::shared_ptr<Surface>;
class Star;
using StarPtr = std::shared_ptr<Star>;
class SolarSystem;
using SolarSystemPtr = std::shared_ptr<SolarSystem>;
class StarDatabase;
using StarDatabasePtr = std::shared_ptr<StarDatabase>;
class Simulation;
using SimulationPtr = std::shared_ptr<Simulation>;
class Timeline;
using TimelinePtr = std::shared_ptr<Timeline>;
class TimelinePhase;
using TimelinePhasePtr = std::shared_ptr<TimelinePhase>;
class Universe;
using UniversePtr = std::shared_ptr<Universe>;
using UniverseConstPtr = std::shared_ptr<const Universe>;

class Image;

// Deep sky objects
class Nebula;
class Galaxy;
class Globular;
class OpenCluster;
