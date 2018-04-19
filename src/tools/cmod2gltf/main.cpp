#include <celmodel/modelfile.h>
#include <iostream>
#include <fstream>
#include <iostream>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

int main(int argc, char** argv) {
    static const std::string CMOD_EXT{ ".cmod" };
    std::cout << "Foo" << std::endl;
    for (auto& p : fs::directory_iterator("C:/Users/bdavi/Git/celestia/resources/models")) {
        fs::path path = p;
        auto ext = path.extension();
        if (CMOD_EXT == path.extension()) {
            std::cout << path << std::endl;
            auto model = cmod::LoadModel(path.string());
            if (!model) {
                std::cerr << "Failed to load model" << std::endl;
            }
        }
    }
    std::cout << "Done" << std::endl;
}



