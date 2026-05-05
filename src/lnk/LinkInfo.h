#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace lecmd {

struct VolumeInfo;

struct NetworkShareInfo {
    uint32_t size = 0;
    uint32_t flags = 0;
    uint32_t netNameOffset = 0;
    uint32_t deviceNameOffset = 0;
    uint32_t networkProviderType = 0;
    std::string networkShareName;
    std::string deviceName;

    std::string GetProviderTypeString() const;
    std::string GetShareFlagsString() const;
};

struct LinkInfo {
    uint32_t size = 0;
    uint32_t headerSize = 0;
    uint32_t flags = 0;
    uint32_t volumeIdOffset = 0;
    uint32_t localBasePathOffset = 0;
    uint32_t commonNetworkRelativeLinkOffset = 0;
    uint32_t commonPathSuffixOffset = 0;
    uint32_t localBasePathOffsetUnicode = 0;
    uint32_t commonPathSuffixOffsetUnicode = 0;

    std::shared_ptr<VolumeInfo> volumeInfo;
    std::shared_ptr<NetworkShareInfo> networkShareInfo;
    std::string localBasePath;
    std::string commonPathSuffix;

    bool HasVolumeID() const { return (flags & 0x0001) != 0; }
    bool HasNetworkShare() const { return (flags & 0x0002) != 0; }

    std::string GetFlagsString() const;
};

} // namespace lecmd
