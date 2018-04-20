#include <iostream>
#include <fstream>
#include <iostream>
#include <celengine/forward.h>
#include <celengine/universe.h>
#include <celengine/stardb.h>
#include <celengine/boundaries.h>
#include <celengine/simulation.h>

#include <celephem/spiceinterface.h>

#include <celapp/configfile.h>
#include <celapp/destination.h>
#include <celapp/celestiacore.h>
#include <Windows.h>

using namespace std;

static void loadCrossIndex(const StarDatabasePtr& starDB, StarDatabase::Catalog catalog, const string& filename) {
    if (!filename.empty()) {
        ifstream xrefFile(filename.c_str(), ios::in | ios::binary);
        if (xrefFile.good()) {
            if (!starDB->loadCrossIndex(catalog, xrefFile))
                cerr << _("Error reading cross index ") << filename << '\n';
            else
                clog << _("Loaded cross index ") << filename << '\n';
        }
    }
}

bool readStars(const UniversePtr& universe, const CelestiaConfig& cfg) {
    StarDetails::SetStarTextures(cfg.starTextures);

    ifstream starNamesFile(cfg.starNamesFile.c_str(), ios::in);
    if (!starNamesFile.good()) {
        cerr << _("Error opening ") << cfg.starNamesFile << '\n';
        return false;
    }

    auto starNameDB = StarNameDatabase::readNames(starNamesFile);
    if (starNameDB == NULL) {
        cerr << _("Error reading star names file\n");
        return false;
    }

    // First load the binary star database file.  The majority of stars
    // will be defined here.
    auto starDB = std::make_shared<StarDatabase>();
    if (!cfg.starDatabaseFile.empty()) {
        ifstream starFile(cfg.starDatabaseFile.c_str(), ios::in | ios::binary);
        if (!starFile.good()) {
            cerr << _("Error opening ") << cfg.starDatabaseFile << '\n';
            return false;
        }

        if (!starDB->loadBinary(starFile)) {
            cerr << _("Error reading stars file\n");
            return false;
        }
    }

    starDB->setNameDatabase(starNameDB);

    loadCrossIndex(starDB, StarDatabase::HenryDraper, cfg.HDCrossIndexFile);
    loadCrossIndex(starDB, StarDatabase::SAO, cfg.SAOCrossIndexFile);
    loadCrossIndex(starDB, StarDatabase::Gliese, cfg.GlieseCrossIndexFile);

    // Next, read any ASCII star catalog files specified in the StarCatalogs
    // list.
    if (!cfg.starCatalogFiles.empty()) {
        for (const auto& catalog : cfg.starCatalogFiles) {
            if (!catalog.empty()) {
                ifstream starFile(catalog, ios::in);
                if (starFile.good()) {
                    starDB->load(starFile, "");
                } else {
                    cerr << _("Error opening star catalog ") << catalog << '\n';
                }
            }
        }
    }

    // Now, read supplemental star files from the extras directories
#if 0
    for (const auto& dir : cfg.extrasDirs) {
        if (!dir.empty()) {
            Directory* dir = OpenDirectory(*iter);

            StarLoader loader(starDB, "star", Content_CelestiaStarCatalog, progressNotifier);
            loader.pushDir(*iter);
            dir->enumFiles(loader, true);

            delete dir;
        }
    }
#endif

    starDB->finish();

    universe->setStarCatalog(starDB);

    return true;
}

void warning(const std::string& warn) {
    std::cerr << warn << std::endl;
}

int main(int argc, char* argv[]) {
    SetDebugVerbosity(5);
    SetCurrentDirectoryA("C:/Users/bdavi/Git/celestia/resources/");

    auto core = std::make_shared<CelestiaCore>();
    core->initSimulation();
    std::cout << "Simulation Loaded" << std::endl;

    for (int i = 0; i < 100; ++i) {
        Sleep(1000);
        core->tick();
    }

    std::cout << "Simulation Done" << std::endl;
    return 0;
}

extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }

#pragma comment(lib, "legacy_stdio_definitions.lib" )
