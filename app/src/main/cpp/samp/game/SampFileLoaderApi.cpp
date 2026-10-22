#include "SampFileLoaderApi.h"

#include "FileLoader.h"

namespace Supremo::FileLoader {

void InstallHooks()
{
    CFileLoader::InjectHooks();
}

}
