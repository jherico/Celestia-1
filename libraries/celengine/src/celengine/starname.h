//
// C++ Interface: starname
//
// Description:
//
//
// Author: Toti <root@totibox>, (C) 2005
//
// Copyright: See COPYING file that comes with this distribution
//
//

#ifndef _STARNAME_H_
#define _STARNAME_H_

#include "name.h"
#include "star.h"

class StarNameDatabase : public NameDatabase<Star> {
public:
    using Pointer = std::shared_ptr<StarNameDatabase>;
    StarNameDatabase(){};

    uint32_t findCatalogNumberByName(const std::string&) const;

    static StarNameDatabase::Pointer readNames(std::istream&);
};

#endif  // _STARNAME_H_
