#include "SampMapSourceInventory.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>

namespace Xyron::MapSource {
namespace {

constexpr size_t kProbeReadLimit = 4096;
constexpr uint32_t kImgEntrySize = 32;
constexpr uint32_t kImgHeaderSize = 8;
constexpr uint32_t kMaxReasonableEntries = 65536;
constexpr size_t kTextLineBufferSize = 768;

void CopyString(char* dst, size_t dstSize, const char* src)
{
    if (!dst || dstSize == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    std::strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

void SetStatus(SourceInventoryResult& result, InventoryStatus status, const char* error)
{
    result.status = status;
    CopyString(result.error, sizeof(result.error), error);
}

uint32_t ReadLe32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8u) |
           (static_cast<uint32_t>(p[2]) << 16u) |
           (static_cast<uint32_t>(p[3]) << 24u);
}

bool ReadPrefix(const char* path, std::vector<uint8_t>& outData)
{
    outData.clear();
    FILE* file = std::fopen(path, "rb");
    if (!file) {
        return false;
    }

    outData.resize(kProbeReadLimit);
    const size_t read = std::fread(outData.data(), 1, outData.size(), file);
    std::fclose(file);
    outData.resize(read);
    return read > 0;
}

char* TrimLeft(char* value)
{
    if (!value) {
        return value;
    }
    while (*value && std::isspace(static_cast<unsigned char>(*value))) {
        ++value;
    }
    return value;
}

void TrimRight(char* value)
{
    if (!value) {
        return;
    }

    size_t len = std::strlen(value);
    while (len > 0 && std::isspace(static_cast<unsigned char>(value[len - 1]))) {
        value[len - 1] = '\0';
        --len;
    }
}

void SanitizeMapTextLine(char* line)
{
    if (!line) {
        return;
    }

    for (char* it = line; *it; ++it) {
        const unsigned char ch = static_cast<unsigned char>(*it);
        if (ch < 32 || *it == ',') {
            *it = ' ';
        }
    }
}

bool IsSkippableMapTextLine(const char* line)
{
    if (!line || line[0] == '\0') {
        return true;
    }
    if (line[0] == '#' || line[0] == ';') {
        return true;
    }

    char token[16] = {};
    if (std::sscanf(line, "%15s", token) != 1) {
        return true;
    }
    for (char* p = token; *p; ++p) {
        *p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
    }

    return !std::strcmp(token, "end") ||
           !std::strcmp(token, "objs") ||
           !std::strcmp(token, "tobj") ||
           !std::strcmp(token, "inst") ||
           !std::strcmp(token, "path") ||
           !std::strcmp(token, "zone") ||
           !std::strcmp(token, "cull") ||
           !std::strcmp(token, "occl") ||
           !std::strcmp(token, "grge") ||
           !std::strcmp(token, "enex") ||
           !std::strcmp(token, "pick") ||
           !std::strcmp(token, "cars") ||
           !std::strcmp(token, "jump") ||
           !std::strcmp(token, "tcyc") ||
           !std::strcmp(token, "auzo");
}

bool InventoryImgFile(const char* path, SourceInventoryResult& result)
{
    FILE* file = std::fopen(path, "rb");
    if (!file) {
        SetStatus(result, InventoryStatus::OpenFailed, "open failed");
        return false;
    }

    uint8_t header[kImgHeaderSize] = {};
    if (std::fread(header, 1, sizeof(header), file) != sizeof(header)) {
        std::fclose(file);
        SetStatus(result, InventoryStatus::ReadFailed, "short img header");
        return false;
    }

    if (std::memcmp(header, "VER2", 4) != 0) {
        std::fclose(file);
        SetStatus(result, InventoryStatus::Unsupported, "img signature is not VER2");
        return false;
    }

    const uint32_t entryCount = ReadLe32(header + 4);
    if (entryCount > kMaxReasonableEntries) {
        std::fclose(file);
        SetStatus(result, InventoryStatus::Unsupported, "img entry count too large");
        return false;
    }

    const uint64_t tableBytes = static_cast<uint64_t>(entryCount) * kImgEntrySize;
    const uint64_t requiredBytes = kImgHeaderSize + tableBytes;
    if (result.sizeBytes < requiredBytes) {
        std::fclose(file);
        SetStatus(result, InventoryStatus::ReadFailed, "img index exceeds file size");
        return false;
    }

    std::vector<uint8_t> indexData(static_cast<size_t>(requiredBytes));
    std::memcpy(indexData.data(), header, sizeof(header));
    if (tableBytes > 0 &&
        std::fread(indexData.data() + kImgHeaderSize, 1, static_cast<size_t>(tableBytes), file) != tableBytes) {
        std::fclose(file);
        SetStatus(result, InventoryStatus::ReadFailed, "short img index");
        return false;
    }
    std::fclose(file);

    ImgIndexSummary summary{};
    if (!ParseImgIndex(indexData.data(), indexData.size(), &summary)) {
        SetStatus(result, InventoryStatus::Unsupported, "img index parse failed");
        return false;
    }

    result.imgEntryCount = summary.entryCount;
    result.imgParsedEntries = summary.parsedEntries;
    result.imgBadNameCount = summary.badNameCount;
    result.imgTotalSectors = summary.totalSectors;
    CopyString(result.imgFirstName, sizeof(result.imgFirstName), summary.firstName);
    CopyString(result.imgLastName, sizeof(result.imgLastName), summary.lastName);
    result.parsed = true;
    SetStatus(result, InventoryStatus::Parsed, nullptr);
    return true;
}

bool InventoryTextMapFile(const char* path, SourceInventoryResult& result)
{
    FILE* file = std::fopen(path, "r");
    if (!file) {
        SetStatus(result, InventoryStatus::OpenFailed, "open failed");
        return false;
    }

    char line[kTextLineBufferSize] = {};
    while (std::fgets(line, sizeof(line), file)) {
        ++result.textLineCount;
        SanitizeMapTextLine(line);
        char* trimmed = TrimLeft(line);
        TrimRight(trimmed);
        if (IsSkippableMapTextLine(trimmed)) {
            ++result.skippedTextLineCount;
            continue;
        }

        bool parsed = false;
        if (result.kind == SourceAssetKind::IdeText) {
            IdeObjectLine ide{};
            parsed = ParseIdeObjectLine(trimmed, &ide);
            if (parsed) {
                ++result.parsedIdeObjectCount;
            }
        } else if (result.kind == SourceAssetKind::IplText) {
            IplInstanceLine ipl{};
            parsed = ParseIplInstanceLine(trimmed, &ipl);
            if (parsed) {
                ++result.parsedIplInstanceCount;
            }
        }

        if (!parsed) {
            ++result.invalidTextLineCount;
        }
    }
    std::fclose(file);

    result.textScanned = true;
    result.parsed = result.parsedIdeObjectCount > 0 || result.parsedIplInstanceCount > 0;
    SetStatus(result, result.parsed ? InventoryStatus::Parsed : InventoryStatus::Unsupported,
              result.parsed ? nullptr : "no map rows parsed");
    return result.parsed;
}

}

const char* SourceAssetKindName(SourceAssetKind kind)
{
    switch (kind) {
        case SourceAssetKind::ImgArchive: return "img";
        case SourceAssetKind::IdeText: return "ide";
        case SourceAssetKind::IplText: return "ipl";
        case SourceAssetKind::DatText: return "dat";
        case SourceAssetKind::ScmScript: return "scm";
        case SourceAssetKind::GxtText: return "gxt";
        case SourceAssetKind::Unknown:
        default:
            return "unknown";
    }
}

const char* InventoryStatusName(InventoryStatus status)
{
    switch (status) {
        case InventoryStatus::MissingPath: return "missing-path";
        case InventoryStatus::StatFailed: return "stat-failed";
        case InventoryStatus::OpenFailed: return "open-failed";
        case InventoryStatus::ReadFailed: return "read-failed";
        case InventoryStatus::Unsupported: return "unsupported";
        case InventoryStatus::Parsed: return "parsed";
        case InventoryStatus::Unknown:
        default:
            return "unknown";
    }
}

bool InventoryMapSourceFile(const char* path, SourceInventoryResult* outResult)
{
    if (!outResult) {
        return false;
    }

    SourceInventoryResult result{};
    CopyString(result.path, sizeof(result.path), path);
    if (!path || path[0] == '\0') {
        SetStatus(result, InventoryStatus::MissingPath, "missing path");
        *outResult = result;
        return false;
    }

    struct stat st {};
    if (stat(path, &st) != 0 || st.st_size < 0) {
        SetStatus(result, InventoryStatus::StatFailed, "stat failed");
        *outResult = result;
        return false;
    }

    result.exists = true;
    result.sizeBytes = static_cast<uint64_t>(st.st_size);
    result.kind = ClassifySourceAssetPath(path);

    std::vector<uint8_t> prefix;
    result.readable = ReadPrefix(path, prefix);
    if (result.readable) {
        result.probe = ProbeSourceAsset(path, prefix.data(), prefix.size());
        if (result.probe.kind != SourceAssetKind::Unknown) {
            result.kind = result.probe.kind;
        }
    }

    if (result.kind == SourceAssetKind::ImgArchive) {
        InventoryImgFile(path, result);
    } else if (result.kind == SourceAssetKind::IdeText || result.kind == SourceAssetKind::IplText) {
        InventoryTextMapFile(path, result);
    } else if (!result.readable) {
        SetStatus(result, InventoryStatus::OpenFailed, "read prefix failed");
    } else {
        SetStatus(result, InventoryStatus::Unsupported, "not a map source file");
    }

    *outResult = result;
    return result.parsed;
}

size_t InventoryMapSourceFiles(const char* const* paths,
                               size_t pathCount,
                               SourceInventoryResult* outResults,
                               size_t resultCapacity)
{
    if (!paths || !outResults || resultCapacity == 0) {
        return 0;
    }

    const size_t count = std::min(pathCount, resultCapacity);
    for (size_t i = 0; i < count; ++i) {
        InventoryMapSourceFile(paths[i], &outResults[i]);
    }
    return count;
}

}
