#include "SampFileLoaderApi.h"

#include "FileLoader.h"

namespace Xyron::FileLoader {

void InstallHooks()
{
    CFileLoader::InjectHooks();
}

}
