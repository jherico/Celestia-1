// asterism.h
//
// Copyright (C) 2001-2008, the Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_ASTERISM_H_
#define _CELENGINE_ASTERISM_H_

#include <string>
#include <vector>
#include <iostream>
#include <memory>

#include <Eigen/Core>

#include <celutil/color.h>

#include "forward.h"

class Asterism {
public:
    using Pointer = std::shared_ptr<Asterism>;
    Asterism(const std::string&);
    ~Asterism();

    using Chain = std::vector<Eigen::Vector3f>;
    using ChainPointer = std::shared_ptr<Chain>;

    std::string getName(bool i18n = false) const;
    uint32_t getChainCount() const;
    const Chain& getChain(uint32_t) const;

    bool getActive() const;
    void setActive(bool _active);

    Color getOverrideColor() const;
    void setOverrideColor(const Color& c);
    void unsetOverrideColor();
    bool isColorOverridden() const;

    void addChain(Chain&);

private:
    std::string name;
    std::string i18nName;
    std::vector<ChainPointer> chains;

    bool active{ true };
    bool useOverrideColor{ false };
    Color color;
};

using AsterismList = std::vector<Asterism::Pointer>;
using AsterismListPtr = std::shared_ptr<AsterismList>;
AsterismListPtr ReadAsterismList(std::istream&, const StarDatabase&);

#endif  // _CELENGINE_ASTERISM_H_
