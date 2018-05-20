//
// C++ Implementation: dsodb
//
// Description:
//
//
// Author: Toti <root@totibox>, (C) 2005
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include "dsodb.h"

#include <algorithm>

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cassert>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <celutil/bytes.h>
#include <celutil/debug.h>
#include <celutil/utf8.h>
#include <celutil/util.h>
#include <celmath/mathlib.h>
#include <celmath/plane.h>
#include <celastro/astro.h>

#include "celestia.h"
#include "parser.h"
#include "parseobject.h"
#include "multitexture.h"
#include "galaxy.h"
#include "globular.h"
#include "opencluster.h"
#include "nebula.h"

using namespace Eigen;
using namespace std;

// 100 Gly - on the order of the current size of the universe
const float DSO_OCTREE_ROOT_SIZE = 1.0e11f;

static const float DSO_OCTREE_MAGNITUDE = 8.0f;
static const float DSO_EXTRA_ROOM = 0.01f;  // Reserve 1% capacity for extra DSOs
                                            // (useful as a complement of binary loaded DSOs)

const char* DSODatabase::FILE_HEADER = "CEL_DSOs";

// Used to sort DSO pointers by catalog number
auto PtrCatalogNumberOrderingPredicate = [](const DeepSkyObject::Pointer& dso0, const DeepSkyObject::Pointer& dso1) -> bool {
    return (dso0->getCatalogNumber() < dso1->getCatalogNumber());
};

DSODatabase::DSODatabase() : capacity(0), octreeRoot(NULL), nextAutoCatalogNumber(0xfffffffe), avgAbsMag(0) {
}

DSODatabase::~DSODatabase() {
}

DeepSkyObject::Pointer DSODatabase::find(const uint32_t catalogNumber) const {
    auto refDSO = std::make_shared<Galaxy>();  //terrible hack !!
    refDSO->setCatalogNumber(catalogNumber);

    auto begin = catalogNumberIndex.begin();
    auto end = catalogNumberIndex.end();
    auto dso = lower_bound(begin, end, refDSO, PtrCatalogNumberOrderingPredicate);

    if (dso != end && (*dso)->getCatalogNumber() == catalogNumber)
        return *dso;
    else
        return NULL;
}

DeepSkyObject::Pointer DSODatabase::find(const string& name) const {
    if (name.empty())
        return NULL;

    if (namesDB != NULL) {
        uint32_t catalogNumber = namesDB->findCatalogNumberByName(name);
        if (catalogNumber != DeepSkyObject::InvalidCatalogNumber)
            return find(catalogNumber);
    }

    return NULL;
}

vector<string> DSODatabase::getCompletion(const string& name) const {
    vector<string> completion;

    // only named DSOs are supported by completion.
    if (!name.empty() && namesDB != NULL)
        return namesDB->getCompletion(name);
    else
        return completion;
}

string DSODatabase::getDSOName(const DeepSkyObjectConstPtr& dso, bool i18n) const {
    uint32_t catalogNumber = dso->getCatalogNumber();
    if (namesDB != NULL) {
        const auto& names = namesDB->getNamesByCatalogNumber(catalogNumber);
        if (!names.empty()) {
            return i18n ? names.front() : _(names.front());
        }
    }

    return "";
}

string DSODatabase::getDSONameList(const DeepSkyObjectConstPtr& dso, const uint32_t maxNames) const {
    string dsoNames;

    uint32_t catalogNumber = dso->getCatalogNumber();

    if (namesDB) {
        const auto& names = namesDB->getNamesByCatalogNumber(catalogNumber);
        concatenate(names.begin(), names.end(), " / ");
    }
    return dsoNames;
}

void DSODatabase::findVisibleDSOs(DSOHandler& dsoHandler,
                                  const Vector3d& obsPos,
                                  const Quaternionf& obsOrient,
                                  float fovY,
                                  float aspectRatio,
                                  float limitingMag) const {
    
    // Compute the bounding planes of an infinite view frustum
    DSOOctree::Frustum frustumPlanes;
    Vector3d planeNormals[5];

    Quaterniond obsOrientd = obsOrient.cast<double>();
    Matrix3d rot = obsOrientd.toRotationMatrix().transpose();
    double h = tan(fovY / 2);
    double w = h * aspectRatio;

    planeNormals[0] = Vector3d(0, 1, -h);
    planeNormals[1] = Vector3d(0, -1, -h);
    planeNormals[2] = Vector3d(1, 0, -w);
    planeNormals[3] = Vector3d(-1, 0, -w);
    planeNormals[4] = Vector3d(0, 0, -1);

    for (int i = 0; i < 5; ++i) {
        planeNormals[i] = rot * planeNormals[i].normalized();
        frustumPlanes[i] = Hyperplane<double, 3>(planeNormals[i], obsPos);
    }

    octreeRoot->processVisibleObjects(dsoHandler, obsPos, frustumPlanes, limitingMag, DSO_OCTREE_ROOT_SIZE);
}

void DSODatabase::findCloseDSOs(DSOHandler& dsoHandler, const Vector3d& obsPos, float radius) const {
    octreeRoot->processCloseObjects(dsoHandler, obsPos, radius, DSO_OCTREE_ROOT_SIZE);
}

bool DSODatabase::load(istream& in, const string& resourcePath) {
    Tokenizer tokenizer(&in);
    Parser parser(&tokenizer);

    while (tokenizer.nextToken() != Tokenizer::TokenEnd) {
        string objType;
        string objName;

        if (tokenizer.getTokenType() != Tokenizer::TokenName) {
            DPRINTF(0, "Error parsing deep sky catalog file.\n");
            return false;
        }
        objType = tokenizer.getNameValue();

        bool autoGenCatalogNumber = true;
        uint32_t objCatalogNumber = DeepSkyObject::InvalidCatalogNumber;
        if (tokenizer.getTokenType() == Tokenizer::TokenNumber) {
            autoGenCatalogNumber = false;
            objCatalogNumber = (uint32_t)tokenizer.getNumberValue();
            tokenizer.nextToken();
        }

        if (autoGenCatalogNumber) {
            objCatalogNumber = nextAutoCatalogNumber--;
        }

        if (tokenizer.nextToken() != Tokenizer::TokenString) {
            DPRINTF(0, "Error parsing deep sky catalog file: bad name.\n");
            return false;
        }
        objName = tokenizer.getStringValue();

        auto objParamsValue = parser.readValue();
        if (objParamsValue == NULL || objParamsValue->getType() != Value::HashType) {
            DPRINTF(0, "Error parsing deep sky catalog entry %s\n", objName.c_str());
            return false;
        }

        const auto& objParams = objParamsValue->getHash();
        assert(objParams != NULL);

        DeepSkyObject::Pointer obj;
        if (compareIgnoringCase(objType, "Galaxy") == 0)
            obj = std::make_shared<Galaxy>();
        else if (compareIgnoringCase(objType, "Globular") == 0)
            obj = std::make_shared<Globular>();
        else if (compareIgnoringCase(objType, "Nebula") == 0)
            obj = std::make_shared<Nebula>();
        else if (compareIgnoringCase(objType, "OpenCluster") == 0)
            obj = std::make_shared<OpenCluster>();

        if (obj && obj->load(objParams, resourcePath)) {
            DSOs.push_back(obj);

            obj->setCatalogNumber(objCatalogNumber);

            if (namesDB != NULL && !objName.empty()) {
                // List of names will replace any that already exist for
                // this DSO.
                namesDB->erase(objCatalogNumber);

                // Iterate through the string for names delimited
                // by ':', and insert them into the DSO database.
                // Note that db->add() will skip empty names.
                string::size_type startPos = 0;
                while (startPos != string::npos) {
                    string::size_type next = objName.find(':', startPos);
                    string::size_type length = string::npos;
                    if (next != string::npos) {
                        length = next - startPos;
                        ++next;
                    }
                    string DSOName = objName.substr(startPos, length);
                    namesDB->add(objCatalogNumber, DSOName);
                    if (DSOName != _(DSOName.c_str()))
                        namesDB->add(objCatalogNumber, _(DSOName.c_str()));
                    startPos = next;
                }
            }
        } else {
            DPRINTF(1, "Bad Deep Sky Object definition--will continue parsing file.\n");
            return false;
        }
    }
    return true;
}

bool DSODatabase::loadBinary(istream&) {
    // TODO: define a binary dso file format
    return true;
}

void DSODatabase::finish() {
    buildOctree();
    buildIndexes();
    calcAvgAbsMag();
    /*
    // Put AbsMag = avgAbsMag for Add-ons without AbsMag entry
    for (int i = 0; i < nDSOs; ++i)
    {
        if(DSOs[i]->getAbsoluteMagnitude() == DSO_DEFAULT_ABS_MAGNITUDE)
            DSOs[i]->setAbsoluteMagnitude((float)avgAbsMag);
    }
    */
    clog << _("Loaded ") << DSOs.size() << _(" deep space objects") << '\n';
}

void DSODatabase::buildOctree() {
    DPRINTF(1, "Sorting DSOs into octree . . .\n");
    float absMag = astro::appToAbsMag(DSO_OCTREE_MAGNITUDE, DSO_OCTREE_ROOT_SIZE * (float)sqrt(3.0));

    // TODO: investigate using a different center--it's possible that more
    // objects end up straddling the base level nodes when the center of the
    // octree is at the origin.
    DynamicOctree<DeepSkyObject, double> root(Vector3d::Zero(), absMag);
    for (const auto& dso : DSOs) {
        root.insertObject(dso, DSO_OCTREE_ROOT_SIZE);
    }

    DPRINTF(1, "Spatially sorting DSOs for improved locality of reference . . .\n");
    std::vector<DeepSkyObject::Pointer> sortedDSOs = DSOs;

    // The spatial sorting part is useless for DSOs since we
    // are storing pointers to objects and not the objects themselves:
    root.rebuildAndSort(octreeRoot, sortedDSOs);

    DPRINTF(1, "%d DSOs total\n", (int)(sortedDSOs.size()));
    DPRINTF(1, "Octree has %d nodes and %d DSOs.\n", 1 + octreeRoot->countChildren(), octreeRoot->countObjects());
    //cout<<"DSOs:  "<< octreeRoot->countObjects()<<"   Nodes:"
    //    <<octreeRoot->countChildren() <<endl;
    // Clean up . . .
    DSOs = sortedDSOs;
}

void DSODatabase::calcAvgAbsMag() {
    auto nDSOeff = size();
    for (const auto& dso : DSOs) {
        double DSOmag = dso->getAbsoluteMagnitude();

        // take only DSO's with realistic AbsMag entry
        // (> DSO_DEFAULT_ABS_MAGNITUDE) into account
        if (DSOmag > DSO_DEFAULT_ABS_MAGNITUDE)
            avgAbsMag += DSOmag;
        else if (nDSOeff > 1)
            nDSOeff--;
        //cout << nDSOs<<"  "<<DSOmag<<"  "<<nDSOeff<<endl;
    }
    avgAbsMag /= (double)nDSOeff;
    //cout<<avgAbsMag<<endl;
}

void DSODatabase::buildIndexes() {
    // This should only be called once for the database
    // assert(catalogNumberIndexes[0] == NULL);

    DPRINTF(1, "Building catalog number indexes . . .\n");

    catalogNumberIndex = DSOs;
    sort(catalogNumberIndex.begin(), catalogNumberIndex.end(), PtrCatalogNumberOrderingPredicate);
}

double DSODatabase::getAverageAbsoluteMagnitude() const {
    return avgAbsMag;
}
