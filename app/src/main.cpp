#include "CelestiaVrApplication.h"

#include <QtCore/QDir>


#if defined(Q_OS_WIN)
#pragma comment(lib, "legacy_stdio_definitions.lib")
extern "C" {
FILE __iob_func[3] = { *stdin, *stdout, *stderr };
}
#endif

// Progress notifier class receives update messages from CelestiaCore
// at startup. This simple implementation just forwards messages on
// to the main Celestia window.

int main(int argc, char* argv[]) {
    return CelestiaVrApplication(argc, argv).exec();
}
