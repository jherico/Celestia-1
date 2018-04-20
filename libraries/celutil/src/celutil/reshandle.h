// reshandle.h
//
// Copyright (C) 2001 Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _RESHANDLE_H_
#define _RESHANDLE_H_

#include <cstdint>

using ResourceHandle = uintptr_t;

#define InvalidResource ((ResourceHandle)~0)

#endif // _RESHANDLE_H_
