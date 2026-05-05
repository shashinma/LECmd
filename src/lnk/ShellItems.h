#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace lecmd {

struct ExtensionBlock {
    uint16_t signature = 0;
    virtual ~ExtensionBlock() = default;
    virtual std::string GetTypeName() const = 0;
};

struct Beef0003Block : ExtensionBlock {
    std::string guid1;
    std::string guid1Folder;
    std::string GetTypeName() const override { return "Beef0003"; }
};

struct MFTInformation {
    std::optional<uint64_t> mftEntryNumber;
    std::optional<uint64_t> mftSequenceNumber;
};

struct Beef0004Block : ExtensionBlock {
    uint16_t version = 0;
    std::string longName;
    std::string localisedName;
    std::optional<std::time_t> createdOnTime;
    std::optional<std::time_t> lastAccessTime;
    MFTInformation mftInfo;
    std::string GetTypeName() const override { return "Beef0004"; }
};

struct Beef001aBlock : ExtensionBlock {
    std::string fileDocumentTypeString;
    std::string GetTypeName() const override { return "Beef001a"; }
};

struct Beef0025Block : ExtensionBlock {
    std::optional<std::time_t> fileTime1;
    std::optional<std::time_t> fileTime2;
    std::string GetTypeName() const override { return "Beef0025"; }
};

struct ShellBag {
    uint8_t type = 0;
    uint16_t size = 0;
    std::string value;
    std::string friendlyName;
    std::vector<std::shared_ptr<ExtensionBlock>> extensionBlocks;

    // For 0x31/0x32/0x74
    std::string shortName;
    std::optional<std::time_t> lastModificationTime;

    // For 0x00/0x1F/0x71 property store
    // Simplified - store as key-value strings
    std::vector<std::pair<std::string, std::string>> propertyStore;

    // For 0x01 drive letter
    std::string driveLetter;

    virtual ~ShellBag() = default;
    virtual std::string GetTypeName() const;
};

std::string ParseShellBagAbsolutePath(const std::vector<std::shared_ptr<ShellBag>>& bags);

} // namespace lecmd
