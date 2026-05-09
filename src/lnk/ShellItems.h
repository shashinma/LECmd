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
    std::string note;
};

struct Beef0004Block : ExtensionBlock {
    uint16_t version = 0;
    int16_t identifier = 0;
    std::string longName;
    std::string localisedName;
    std::optional<std::time_t> createdOnTime;
    std::optional<std::time_t> lastAccessTime;
    MFTInformation mftInfo;
    std::string GetTypeName() const override { return "Beef0004"; }
    std::string GetOsHint() const;
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

struct Beef0005Block : ExtensionBlock {
    std::string GetTypeName() const override { return "Beef0005"; }
};

struct Beef0026Block : ExtensionBlock {
    uint16_t version = 0;
    std::optional<std::time_t> createdOn;
    std::optional<std::time_t> lastModified;
    std::optional<std::time_t> lastAccessed;
    // For PropertyStore variant
    std::vector<std::pair<std::string, std::string>> propertyStore;
    int16_t versionOffset = 0;
    std::string GetTypeName() const override { return "Beef0026"; }
};

struct BeefUnknownBlock : ExtensionBlock {
    uint16_t version = 0;
    std::vector<uint8_t> rawBytes;
    std::string GetTypeName() const override { return "BeefUnknown"; }
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

    // For 0x00 special cases (FTP, MTP, CD Burn, URL)
    std::optional<std::time_t> createdOnTime;
    std::optional<std::time_t> lastAccessTime;
    std::optional<std::time_t> ftpFolderTime;
    std::string fullUrl;
    std::vector<std::string> mtpGuids;
    std::string storageIdName;
    std::string fileSystemName;
    std::string classId;
    std::string mtpType1GuidName;

    // For 0x1F Windows Backup
    std::optional<std::time_t> modifiedDateFromBackup;
    std::optional<std::time_t> createdDateFromBackup;
    std::optional<std::time_t> backupDateTime;
    std::optional<std::time_t> backupUnknownDateTime;

    // For 0x61 URI
    std::string uri;
    std::string userName;
    uint8_t uriFlags = 0;
    std::optional<std::time_t> fileTime1;

    // For 0x2E Control Panel / User profile
    std::string category;
    std::string devicePath;

    // For 0xC3
    uint8_t c3ClassType = 0;
    uint8_t c3Flags = 0;

    // For 0x40 Network
    std::string networkDesc;

    virtual ~ShellBag() = default;
    virtual std::string GetTypeName() const;
};

std::string ParseShellBagAbsolutePath(const std::vector<std::shared_ptr<ShellBag>>& bags);

} // namespace lecmd
