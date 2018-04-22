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

int main(int argc, char* argv[]) {

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

