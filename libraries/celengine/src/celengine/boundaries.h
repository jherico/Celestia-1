// boundaries.h
//
// Copyright (C) 2002-2009, the Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_BOUNDARIES_H_
#define _CELENGINE_BOUNDARIES_H_

#include <Eigen/Core>
#include <string>
#include <vector>
#include <iostream>
#include <memory>

class ConstellationBoundaries {
public:
    using Pointer = std::shared_ptr<ConstellationBoundaries>;
    ConstellationBoundaries();
    ~ConstellationBoundaries();

    using Chain = std::vector<Eigen::Vector3f>;
    using ChainPtr = std::shared_ptr<Chain>;

    void moveto(float ra, float dec);
    void lineto(float ra, float dec);

private:
    ChainPtr currentChain;
    std::vector<ChainPtr> chains;
};

ConstellationBoundaries::Pointer ReadBoundaries(std::istream&);

#endif  // _CELENGINE_BOUNDARIES_H_
