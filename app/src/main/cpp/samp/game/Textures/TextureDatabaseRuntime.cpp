//
// Created by x1y2z on 11.01.2023.
//

#include "TextureDatabaseRuntime.h"
#include "../../main.h"
#include "../GTASAEngineApi.h"
#include <cstring>

#if !VER_x32
static bool (*TextureDatabase__LoadThumbs)(TextureDatabase *thiz, TextureDatabaseFormat forFormat, bool setEntries);
static void (*TextureDatabaseRuntime__SortEntries)(TextureDatabaseRuntime *thiz, bool fullSort);

static bool TextureDatabase__LoadThumbs_hook(TextureDatabase *thiz,
                                              TextureDatabaseFormat forFormat,
                                              bool setEntries)
{
    if (!TextureDatabase__LoadThumbs) {
        FLog("[TEXDB64] LoadThumbs original is unavailable");
        return false;
    }

    // LoadThumbs reads the selected thumbnail bank.  Do not modify the native
    // entry count here: SortEntries consumes that count immediately afterward.
    const bool result = TextureDatabase__LoadThumbs(thiz, forFormat, setEntries);

    if (thiz) {
        FLog("[TEXDB64] LoadThumbs name=%s fmt=%d ok=%d entries=%u thumbs=%u",
             thiz->name ? thiz->name : "?",
             static_cast<int>(forFormat),
             result ? 1 : 0,
             thiz->entries.numEntries,
             thiz->thumbs[forFormat].numEntries);
    }

    return result;
}

static bool TextureDatabaseRuntime_IsWorldTextureDb(const char *name)
{
    return name &&
           (!std::strcmp(name, "gta3") ||
            !std::strcmp(name, "gta_int") ||
            !std::strcmp(name, "txd") ||
            !std::strcmp(name, "mobile"));
}

static void TextureDatabaseRuntime__SortEntries_hook(TextureDatabaseRuntime *thiz, bool fullSort)
{
    if (!TextureDatabaseRuntime__SortEntries) {
        FLog("[TEXDB64] SortEntries original is unavailable");
        return;
    }

    // Keep this wrapper observational only.  Native SortEntries rebuilds the
    // hash and sorted-entry arrays and must receive the original metadata.
    TextureDatabaseRuntime__SortEntries(thiz, fullSort);
}
#endif

TextureDatabaseRuntime* TextureDatabaseRuntime::Load(const char *withName, bool fullyLoad, TextureDatabaseFormat forcedFormat)
{
#if !VER_x32
    FLog("[TEXDB64] load begin name=%s forced=%d full=%d",
         withName ? withName : "?",
         static_cast<int>(forcedFormat),
         fullyLoad ? 1 : 0);
#endif
    TextureDatabaseRuntime* db = GTASAEngineApi::TextureDatabaseRuntimeLoadNative(
        withName,
        fullyLoad,
        static_cast<int32>(forcedFormat));
#if !VER_x32
    if (db) {
        FLog("[TEXDB64] load name=%s forced=%d full=%d result=0x%llX loaded=%d entries=%u thumbs={unc:%u,dxt:%u,x360:%u,ps3:%u,pvr:%u,etc:%u} hashes=%u",
             withName ? withName : "?",
             static_cast<int>(forcedFormat),
             fullyLoad ? 1 : 0,
             static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(db)),
             static_cast<int>(db->loadedFormat),
             db->entries.numEntries,
             db->thumbs[0].numEntries,
             db->thumbs[1].numEntries,
             db->thumbs[2].numEntries,
             db->thumbs[3].numEntries,
             db->thumbs[4].numEntries,
             db->thumbs[5].numEntries,
             db->numHashes);
    } else {
        FLog("[TEXDB64] load name=%s forced=%d full=%d result=null",
             withName ? withName : "?",
             static_cast<int>(forcedFormat),
             fullyLoad ? 1 : 0);
    }
#endif
    return db;
}

void TextureDatabaseRuntime::Register(TextureDatabaseRuntime *toRegister) {
    GTASAEngineApi::TextureDatabaseRuntimeRegisterNative(toRegister);
//    if (std::find(registered.dataPtr, registered.dataPtr + registered.numEntries, toRegister) != registered.dataPtr + registered.numEntries) {
//        return; // ��� ���������������, �������
//    }
//
//    if (registered.numAlloced < registered.numEntries + 1) {
//        size_t newAlloc = ((3 * (registered.numEntries + 1)) >> 1) + 3;
//        TextureDatabaseRuntime** newData = static_cast<TextureDatabaseRuntime**>(malloc(sizeof(TextureDatabaseRuntime*) * newAlloc));
//
//        if (registered.dataPtr) {
//            std::memcpy(newData, registered.dataPtr, sizeof(TextureDatabaseRuntime*) * registered.numEntries);
//            free(registered.dataPtr);
//        }
//
//        registered.dataPtr = newData;
//        registered.numAlloced = newAlloc;
//    }
//
//    registered.dataPtr[registered.numEntries++] = toRegister;
}

void TextureDatabaseRuntime::Unregister(TextureDatabaseRuntime *toUnregister) {
    GTASAEngineApi::TextureDatabaseRuntimeUnregisterNative(toUnregister);
}

RwTexture* TextureDatabaseRuntime::GetTexture(const char *name) {
    return GTASAEngineApi::TextureDatabaseRuntimeGetTextureNative(name);
}

RwTexture* TextureDatabaseRuntime::GetRWTexture(int32 index) {
    return GTASAEngineApi::TextureDatabaseRuntimeGetRWTextureNative(this, index);
}

void TextureDatabaseRuntime::SetAsRendered(uint32 index) {
    GTASAEngineApi::TextureDatabaseRuntimeSetAsRenderedNative(this, index);
}

void TextureDatabaseRuntime::LoadFullTexture(uint32 index) {
    GTASAEngineApi::TextureDatabaseRuntimeLoadFullTextureNative(this, index);
}

void TextureDatabaseRuntime::UpdateStreaming(float deltaTime, bool flush) {
    GTASAEngineApi::TextureDatabaseRuntimeUpdateStreamingNative(deltaTime, flush);
}

TextureDatabaseRuntime* TextureDatabaseRuntime::GetDatabase(const char *dbName) {
    return GTASAEngineApi::TextureDatabaseRuntimeGetDatabaseNative(dbName);
//    for (unsigned int i = 0; i < TextureDatabaseRuntime::loaded.numEntries; ++i) {
//        TextureDatabaseRuntime *currentDatabase = TextureDatabaseRuntime::loaded.dataPtr[i];
//        if (strcmp(currentDatabase->name, dbName) == 0) {
//            return currentDatabase;
//        }
//    }
//
//    return nullptr;
}

void TextureDatabaseRuntime::InjectHooks() {
#if !VER_x32
    // Keep the native SortEntries implementation untouched.  It owns the
    // hash/sorted-entry rebuild and must see the original entry cardinality;
    // the former wrapper could alter that state before the native call.
    // Keep the texture database loader completely native while diagnosing
    // external package compatibility.  The wrappers are observational only,
    // but even an inline hook here can change a loader's ABI-sensitive state.
    GTASAEngineApi::InstallTextureDatabaseRuntimeHooksNative(
        0,
        nullptr,
        0,
        nullptr);
#endif
}

RwBool TextureAnnihilate(RwTexture *texture) {
    return GTASAEngineApi::TextureAnnihilateNative(texture);
}
