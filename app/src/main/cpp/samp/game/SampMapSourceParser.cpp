#include "SampMapSourceParser.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace Xyron::MapSource {
namespace {

constexpr uint32_t kImgEntrySize = 32;
constexpr uint32_t kImgHeaderSize = 8;
constexpr uint32_t kMaxReasonableEntries = 65536;

uint32_t ReadLe32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8u) |
           (static_cast<uint32_t>(p[2]) << 16u) |
           (static_cast<uint32_t>(p[3]) << 24u);
}

void CopyFixedName(char* dst, size_t dstSize, const uint8_t* src, size_t srcSize)
{
    if (!dst || dstSize == 0) {
        return;
    }

    size_t n = 0;
    while (n + 1 < dstSize && n < srcSize && src[n] != 0) {
        dst[n] = static_cast<char>(src[n]);
        ++n;
    }
    dst[n] = '\0';
}

bool HasSuspiciousNameBytes(const char* value)
{
    if (!value || value[0] == '\0') {
        return true;
    }

    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
        if (*p < 32 || *p > 126) {
            return true;
        }
    }
    return false;
}

bool EndsWithNoCase(const char* value, const char* suffix)
{
    if (!value || !suffix) {
        return false;
    }

    const size_t valueLen = std::strlen(value);
    const size_t suffixLen = std::strlen(suffix);
    if (suffixLen > valueLen) {
        return false;
    }

    const char* tail = value + valueLen - suffixLen;
    for (size_t i = 0; i < suffixLen; ++i) {
        const unsigned char a = static_cast<unsigned char>(tail[i]);
        const unsigned char b = static_cast<unsigned char>(suffix[i]);
        if (std::tolower(a) != std::tolower(b)) {
            return false;
        }
    }
    return true;
}

bool BufferLooksText(const uint8_t* data, size_t size)
{
    if (!data || size == 0) {
        return false;
    }

    const size_t sampleSize = size < 512 ? size : 512;
    size_t printable = 0;
    size_t zeroes = 0;
    for (size_t i = 0; i < sampleSize; ++i) {
        const uint8_t ch = data[i];
        if (ch == 0) {
            ++zeroes;
            continue;
        }
        if (ch == '\n' || ch == '\r' || ch == '\t' || (ch >= 32 && ch <= 126)) {
            ++printable;
        }
    }

    return zeroes < sampleSize / 8 && printable >= (sampleSize * 7) / 8;
}

}

SourceAssetKind ClassifySourceAssetPath(const char* path)
{
    if (!path || path[0] == '\0') {
        return SourceAssetKind::Unknown;
    }

    if (EndsWithNoCase(path, ".img")) {
        return SourceAssetKind::ImgArchive;
    }
    if (EndsWithNoCase(path, ".ide")) {
        return SourceAssetKind::IdeText;
    }
    if (EndsWithNoCase(path, ".ipl")) {
        return SourceAssetKind::IplText;
    }
    if (EndsWithNoCase(path, ".dat")) {
        return SourceAssetKind::DatText;
    }
    if (EndsWithNoCase(path, ".scm")) {
        return SourceAssetKind::ScmScript;
    }
    if (EndsWithNoCase(path, ".gxt")) {
        return SourceAssetKind::GxtText;
    }
    return SourceAssetKind::Unknown;
}

SourceAssetProbe ProbeSourceAsset(const char* path, const uint8_t* data, size_t size)
{
    SourceAssetProbe probe{};
    probe.kind = ClassifySourceAssetPath(path);
    probe.hasVer2Signature = data && size >= 4 && std::memcmp(data, "VER2", 4) == 0;
    probe.probablyText = BufferLooksText(data, size);

    if (probe.kind == SourceAssetKind::ImgArchive || probe.hasVer2Signature) {
        ImgIndexSummary summary{};
        probe.imgIndexParseable = ParseImgIndex(data, size, &summary);
        if (probe.imgIndexParseable) {
            probe.kind = SourceAssetKind::ImgArchive;
            probe.imgEntryCount = summary.entryCount;
            probe.badNameCount = summary.badNameCount;
            probe.imgTotalSectors = summary.totalSectors;
        }
    }

    return probe;
}

bool ParseImgIndex(const uint8_t* data, size_t size, ImgIndexSummary* outSummary)
{
    if (!outSummary) {
        return false;
    }

    *outSummary = ImgIndexSummary{};
    if (!data || size < kImgHeaderSize) {
        return false;
    }

    if (std::memcmp(data, "VER2", 4) != 0) {
        return false;
    }

    const uint32_t entryCount = ReadLe32(data + 4);
    if (entryCount > kMaxReasonableEntries) {
        return false;
    }

    const uint64_t tableBytes = static_cast<uint64_t>(entryCount) * kImgEntrySize;
    if (tableBytes > static_cast<uint64_t>(size - kImgHeaderSize)) {
        return false;
    }

    outSummary->entryCount = entryCount;
    const uint8_t* entry = data + kImgHeaderSize;
    for (uint32_t i = 0; i < entryCount; ++i, entry += kImgEntrySize) {
        ImgEntry parsed{};
        parsed.offsetSector = ReadLe32(entry);
        parsed.sizeSectors = ReadLe32(entry + 4);
        CopyFixedName(parsed.name, sizeof(parsed.name), entry + 8, 24);

        if (i == 0) {
            std::memcpy(outSummary->firstName, parsed.name, sizeof(outSummary->firstName));
        }
        std::memcpy(outSummary->lastName, parsed.name, sizeof(outSummary->lastName));

        outSummary->totalSectors += parsed.sizeSectors;
        if (HasSuspiciousNameBytes(parsed.name)) {
            ++outSummary->badNameCount;
        }
        ++outSummary->parsedEntries;
    }

    outSummary->ok = true;
    return true;
}

bool ParseIdeObjectLine(const char* line, IdeObjectLine* out)
{
    if (!line || !out) {
        return false;
    }

    IdeObjectLine parsed{};
    const int read = std::sscanf(
        line,
        " %d %31s %31s %f %u",
        &parsed.modelId,
        parsed.modelName,
        parsed.textureName,
        &parsed.drawDistance,
        &parsed.flags);
    if (read < 4) {
        return false;
    }
    if (read < 5) {
        parsed.flags = 0;
    }

    *out = parsed;
    return parsed.modelId >= 0 && parsed.modelName[0] != '\0' && parsed.textureName[0] != '\0';
}

bool ParseIplInstanceLine(const char* line, IplInstanceLine* out)
{
    if (!line || !out) {
        return false;
    }

    IplInstanceLine parsed{};
    const int read = std::sscanf(
        line,
        " %d %31s %d %f %f %f %f %f %f %f %d",
        &parsed.modelId,
        parsed.modelName,
        &parsed.interior,
        &parsed.position[0],
        &parsed.position[1],
        &parsed.position[2],
        &parsed.rotation[0],
        &parsed.rotation[1],
        &parsed.rotation[2],
        &parsed.rotation[3],
        &parsed.lod);
    if (read != 11) {
        return false;
    }

    *out = parsed;
    return parsed.modelId >= 0 && parsed.modelName[0] != '\0';
}

}
