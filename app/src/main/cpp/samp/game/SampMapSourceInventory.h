#pragma once

#include "SampMapSourceParser.h"

#include <cstddef>
#include <cstdint>

namespace Xyron::MapSource {

constexpr size_t kInventoryPathSize = 384;
constexpr size_t kInventoryErrorSize = 96;

enum class InventoryStatus : uint8_t {
    Unknown,
    MissingPath,
    StatFailed,
    OpenFailed,
    ReadFailed,
    Unsupported,
    Parsed
};

struct SourceInventoryResult {
    InventoryStatus status = InventoryStatus::Unknown;
    SourceAssetKind kind = SourceAssetKind::Unknown;
    SourceAssetProbe probe{};
    bool exists = false;
    bool readable = false;
    bool parsed = false;
    bool textScanned = false;
    uint64_t sizeBytes = 0;

    uint32_t imgEntryCount = 0;
    uint32_t imgParsedEntries = 0;
    uint32_t imgBadNameCount = 0;
    uint64_t imgTotalSectors = 0;
    char imgFirstName[25] = {};
    char imgLastName[25] = {};

    uint32_t textLineCount = 0;
    uint32_t skippedTextLineCount = 0;
    uint32_t parsedIdeObjectCount = 0;
    uint32_t parsedIplInstanceCount = 0;
    uint32_t invalidTextLineCount = 0;

    char path[kInventoryPathSize] = {};
    char error[kInventoryErrorSize] = {};
};

const char* SourceAssetKindName(SourceAssetKind kind);
const char* InventoryStatusName(InventoryStatus status);
bool InventoryMapSourceFile(const char* path, SourceInventoryResult* outResult);
size_t InventoryMapSourceFiles(const char* const* paths,
                               size_t pathCount,
                               SourceInventoryResult* outResults,
                               size_t resultCapacity);

}
