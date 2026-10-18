#pragma once

#include <cstddef>
#include <cstdint>

namespace Xyron::MapSource {

enum class SourceAssetKind : uint8_t {
    Unknown,
    ImgArchive,
    IdeText,
    IplText,
    DatText,
    ScmScript,
    GxtText
};

struct SourceAssetProbe {
    SourceAssetKind kind = SourceAssetKind::Unknown;
    bool hasVer2Signature = false;
    bool imgIndexParseable = false;
    bool probablyText = false;
    uint32_t imgEntryCount = 0;
    uint32_t badNameCount = 0;
    uint64_t imgTotalSectors = 0;
};

struct ImgEntry {
    uint32_t offsetSector = 0;
    uint32_t sizeSectors = 0;
    char name[25] = {};
};

struct ImgIndexSummary {
    bool ok = false;
    uint32_t entryCount = 0;
    uint32_t parsedEntries = 0;
    uint32_t badNameCount = 0;
    uint64_t totalSectors = 0;
    char firstName[25] = {};
    char lastName[25] = {};
};

struct IdeObjectLine {
    int32_t modelId = -1;
    char modelName[32] = {};
    char textureName[32] = {};
    float drawDistance = 0.0f;
    uint32_t flags = 0;
};

struct IplInstanceLine {
    int32_t modelId = -1;
    char modelName[32] = {};
    int32_t interior = 0;
    float position[3] = {};
    float rotation[4] = {};
    int32_t lod = -1;
};

SourceAssetKind ClassifySourceAssetPath(const char* path);
SourceAssetProbe ProbeSourceAsset(const char* path, const uint8_t* data, size_t size);
bool ParseImgIndex(const uint8_t* data, size_t size, ImgIndexSummary* outSummary);
bool ParseIdeObjectLine(const char* line, IdeObjectLine* out);
bool ParseIplInstanceLine(const char* line, IplInstanceLine* out);

}
