#pragma once

#include <vector>
#include <memory>

struct FavoritesEntry;
using FavoritesList = std::vector<FavoritesEntry>;
class Destination;
using DestinationList = std::vector<Destination>;
class CelestiaCore;
using CelestiaCorePtr = std::shared_ptr<CelestiaCore>;
class CelestiaState;