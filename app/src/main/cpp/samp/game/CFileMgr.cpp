//
// Created by x1y2z on 14.04.2023.
//

#include "CFileMgr.h"
#include "GTASAEngineApi.h"
#include "main.h"

namespace {
bool FileExistsReadable(const char* path)
{
    if (!path || !path[0]) {
        return false;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    fclose(file);
    return true;
}

void AddSampArchiveStorage()
{
    char archivePath[256]{};

    if (g_pszStorage && g_pszStorage[0]) {
        snprintf(archivePath, sizeof(archivePath), "%ssamp.data", g_pszStorage);
        if (!FileExistsReadable(archivePath)) {
            archivePath[0] = '\0';
        }
    }

    if (!archivePath[0] && FileExistsReadable("/storage/emulated/0/Android/data/com.xyron.game/samp.data")) {
        snprintf(archivePath, sizeof(archivePath), "%s", "/storage/emulated/0/Android/data/com.xyron.game/samp.data");
    }

    if (!archivePath[0] && FileExistsReadable(SAMP_ARCHIVE_PATH)) {
        snprintf(archivePath, sizeof(archivePath), "%s", SAMP_ARCHIVE_PATH);
    }

    if (!archivePath[0]) {
        FLog("[FILE_FIX64] skip SAMP archive: no readable samp.data");
        return;
    }

    uintptr_t zipFile = GTASAEngineApi::ZipFileCreateNative(archivePath);
    if (zipFile) {
        GTASAEngineApi::ZipAddStorageNative(zipFile);
        FLog("[FILE_FIX64] SAMP archive storage added: %s", archivePath);
    } else {
        FLog("[FILE_FIX64] SAMP archive create failed: %s", archivePath);
    }
}
}

void CFileMgr::SetDir(const char *path) {
    GTASAEngineApi::SetFileManagerDir(path);
}

FILE* CFileMgr::OpenFile(const char *path, const char *mode) {
    sprintf(ms_path, "%s%s", g_pszStorage, path);

    auto file = fopen(ms_path, mode);

    if(!file) {
        FLog("Fail open file %s", ms_path);
    }
    return file;
}

int32_t CFileMgr::CloseFile(FILE* file) {
    return fclose(file);
}

void CFileMgr::Initialise() {
    AddSampArchiveStorage();
    GTASAEngineApi::InitialiseFileManagerNative();
}
