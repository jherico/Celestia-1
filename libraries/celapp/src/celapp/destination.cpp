// destination.cpp
//
// Copyright (C) 2001, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include <algorithm>
#include <celutil/debug.h>
#include <celutil/util.h>
#include <celastro/astro.h>
#include <celengine/celestia.h>
#include <celengine/parser.h>
#include "destination.h"

using namespace std;


bool ReadDestinationList(istream& in, DestinationList& destinations) {
    Tokenizer tokenizer(&in);
    Parser parser(&tokenizer);

    while (tokenizer.nextToken() != Tokenizer::TokenEnd) {
        if (tokenizer.getTokenType() != Tokenizer::TokenBeginGroup) {
            DPRINTF(0, "Error parsing destinations file.\n");
            destinations.clear();
            return false;
        }
        tokenizer.pushBack();

        auto destValue = parser.readValue();
        if (destValue == NULL || destValue->getType() != Value::HashType) {
            DPRINTF(0, "Error parsing destination.\n");
            destinations.clear();
            return false;
        }

        auto destParams = destValue->getHash();
        Destination dest;

        if (!destParams->getString("Name", dest.name)) {
            DPRINTF(1, "Skipping unnamed destination\n");
        } else {
            destParams->getString("Target", dest.target);
            destParams->getString("Description", dest.description);
            destParams->getNumber("Distance", dest.distance);

            // Default unit of distance is the light year
            string distanceUnits;
            if (destParams->getString("DistanceUnits", distanceUnits)) {
                if (!compareIgnoringCase(distanceUnits, "km"))
                    dest.distance = astro::kilometersToLightYears(dest.distance);
                else if (!compareIgnoringCase(distanceUnits, "au"))
                    dest.distance = astro::AUtoLightYears(dest.distance);
            }

            destinations.push_back(dest);
        }
    }
    return true;
}
