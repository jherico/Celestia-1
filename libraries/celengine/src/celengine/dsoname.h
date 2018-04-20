//
// C++ Interface: dsoname
//
// Description:
//
//
// Author: Toti <root@totibox>, (C) 2005
//
// Copyright: See COPYING file that comes with this distribution
//
//

#ifndef _DSONAME_H_
#define _DSONAME_H_

#include "name.h"
#include "deepskyobj.h"

class DSONameDatabase : public NameDatabase<DeepSkyObject> {
public:
    using Pointer = std::shared_ptr<DSONameDatabase>;
    DSONameDatabase(){};

    uint32_t findCatalogNumberByName(const std::string&) const;
};

#endif  // _DSONAME_H_
