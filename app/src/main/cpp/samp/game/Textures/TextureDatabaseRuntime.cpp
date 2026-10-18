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

static uint32_t TextureDatabaseRuntime_GetEtcThumbLimit(const TextureDatabase *db)
{
    if (!db || !db->name) {
        return 0;
    }

    // The shipped gta3 ETC database is shorter than gta3.txt on arm64.  If the
    // native loader uses the full entry count it clears the ETC thumbs and
    // crashes before streaming can render the world textures.
    if (!std::strcmp(db->name, "gta3")) {
        return 12449;
    }

    return 0;
}

static bool TextureDatabase__LoadThumbs_hook(TextureDatabase *thiz, TextureDatabaseFormat forFormat, bool setEntries)
{
    const uint32_t originalEntries = thiz ? thiz->entries.numEntries : 0;
    const uint32_t etcLimit = (forFormat == TextureDatabaseFormat::DF_ETC)
                                  ? TextureDatabaseRuntime_GetEtcThumbLimit(thiz)
                                  : 0;

    if (etcLimit > 0 && originalEntries > etcLimit) {
        FLog("[TEXDB64] trim entries before LoadThumbs name=%s fmt=%d entries=%u limit=%u",
             thiz->name ? thiz->name : "?",
             static_cast<int>(forFormat),
             originalEntries,
             etcLimit);

        thiz->entries.numEntries = etcLimit;
    }

    const bool result = TextureDatabase__LoadThumbs(thiz, forFormat, setEntries);

    if (thiz && forFormat == TextureDatabaseFormat::DF_ETC) {
        FLog("[TEXDB64] LoadThumbs result name=%s fmt=%d ok=%d entries=%u thumbs=%u originalEntries=%u",
             thiz->name ? thiz->name : "?",
             static_cast<int>(forFormat),
             result ? 1 : 0,
             thiz->entries.numEntries,
             thiz->thumbs[TextureDatabaseFormat::DF_ETC].numEntries,
             originalEntries);
    }

    if (!result && etcLimit > 0 && originalEntries > etcLimit) {
        thiz->entries.numEntries = originalEntries;
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
#if !VER_x32
    if (thiz && TextureDatabaseRuntime_IsWorldTextureDb(thiz->name)) {
        FLog("[TEXDB64] sort begin name=%s loaded=%d fullSort=%d entries=%u thumbs={dxt:%u,pvr:%u,etc:%u}",
             thiz->name ? thiz->name : "?",
             static_cast<int>(thiz->loadedFormat),
             fullSort ? 1 : 0,
             thiz->entries.numEntries,
             thiz->thumbs[TextureDatabaseFormat::DF_DXT].numEntries,
             thiz->thumbs[TextureDatabaseFormat::DF_PVR].numEntries,
             thiz->thumbs[TextureDatabaseFormat::DF_ETC].numEntries);
    }
#endif

    if (thiz &&
        thiz->loadedFormat == TextureDatabaseFormat::DF_ETC &&
        TextureDatabaseRuntime_IsWorldTextureDb(thiz->name)) {
        auto &etcThumbs = thiz->thumbs[TextureDatabaseFormat::DF_ETC];

        if (etcThumbs.numEntries > 0 && thiz->entries.numEntries > etcThumbs.numEntries) {
            FLog("[TEXDB64] trim ETC entries before SortEntries name=%s entries=%u etcThumbs=%u",
                 thiz->name ? thiz->name : "?",
                 thiz->entries.numEntries,
                 etcThumbs.numEntries);

            thiz->entries.numEntries = etcThumbs.numEntries;
        }
    }

    TextureDatabaseRuntime__SortEntries(thiz, fullSort);

#if !VER_x32
    if (thiz && TextureDatabaseRuntime_IsWorldTextureDb(thiz->name)) {
        FLog("[TEXDB64] sort end name=%s entries=%u hashes=%u",
             thiz->name ? thiz->name : "?",
             thiz->entries.numEntries,
             thiz->numHashes);
    }
#endif
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
    GTASAEngineApi::InstallTextureDatabaseRuntimeHooksNative(
        reinterpret_cast<uintptr_t>(&TextureDatabase__LoadThumbs_hook),
        reinterpret_cast<void**>(&TextureDatabase__LoadThumbs),
        reinterpret_cast<uintptr_t>(&TextureDatabaseRuntime__SortEntries_hook),
        reinterpret_cast<void**>(&TextureDatabaseRuntime__SortEntries));
#endif
}

RwBool TextureAnnihilate(RwTexture *texture) {
    return GTASAEngineApi::TextureAnnihilateNative(texture);
}
