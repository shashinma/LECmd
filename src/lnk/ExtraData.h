#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace lecmd {

struct ShellBag;

struct ExtraDataBase {
    uint32_t size = 0;
    uint32_t signature = 0;
    virtual ~ExtraDataBase() = default;
    virtual std::string GetTypeName() const = 0;
};

struct ConsoleDataBlock : ExtraDataBase {
    uint16_t fillAttributes = 0;
    uint16_t popupFillAttributes = 0;
    uint16_t screenWidthBufferSize = 0;
    uint16_t screenHeightBufferSize = 0;
    uint16_t windowWidth = 0;
    uint16_t windowHeight = 0;
    uint16_t windowOriginX = 0;
    uint16_t windowOriginY = 0;
    int32_t reserved0 = 0;
    int32_t reserved1 = 0;
    uint16_t fontSize = 0;
    uint32_t fontFamily = 0;
    uint16_t fontWeight = 0;
    bool isBold = false;
    std::string faceName;
    uint32_t cursorSize = 0;
    uint32_t fullScreen = 0;
    uint32_t quickEdit = 0;
    uint32_t insertMode = 0;
    uint32_t autoPosition = 0;
    uint32_t historyBufferSize = 0;
    uint32_t numberOfHistoryBuffers = 0;
    uint32_t historyNoDup = 0;
    std::vector<uint32_t> colorTable;
    std::string GetTypeName() const override { return "ConsoleDataBlock"; }
};

struct ConsoleFEDataBlock : ExtraDataBase {
    uint32_t codePage = 0;
    std::string GetTypeName() const override { return "ConsoleFEDataBlock"; }
};

struct DarwinDataBlock : ExtraDataBase {
    std::string applicationIdentifierAscii;
    std::string applicationIdentifierUnicode;
    std::string productCode;
    std::string featureName;
    std::string componentId;
    std::string GetTypeName() const override { return "DarwinDataBlock"; }
};

struct EnvironmentVariableDataBlock : ExtraDataBase {
    std::string environmentVariablesAscii;
    std::string environmentVariablesUnicode;
    std::string GetTypeName() const override { return "EnvironmentVariableDataBlock"; }
};

struct IconEnvironmentDataBlock : ExtraDataBase {
    std::string iconPathAscii;
    std::string iconPathUni;
    std::string GetTypeName() const override { return "IconEnvironmentDataBlock"; }
};

struct KnownFolderDataBlock : ExtraDataBase {
    std::string knownFolderId;
    uint32_t offset = 0;
    std::string knownFolderName;
    std::string GetTypeName() const override { return "KnownFolderDataBlock"; }
};

struct PropertySheet;

struct PropertyStoreDataBlock : ExtraDataBase {
    std::vector<PropertySheet> sheets;
    std::string GetTypeName() const override { return "PropertyStoreDataBlock"; }
};

struct ShimDataBlock : ExtraDataBase {
    std::string layerName;
    std::string GetTypeName() const override { return "ShimDataBlock"; }
};

struct SpecialFolderDataBlock : ExtraDataBase {
    uint32_t specialFolderId = 0;
    uint32_t offset = 0;
    std::string GetTypeName() const override { return "SpecialFolderDataBlock"; }
};

struct TrackerDataBaseBlock : ExtraDataBase {
    int32_t version = 0;
    std::string machineId;
    std::string macAddress;
    std::optional<std::time_t> creationTime;
    std::string volumeDroid;
    std::string volumeDroidBirth;
    std::string fileDroid;
    std::string fileDroidBirth;
    std::string GetTypeName() const override { return "TrackerDataBaseBlock"; }
};

struct VistaAndAboveIdListDataBlock : ExtraDataBase {
    std::vector<std::shared_ptr<ShellBag>> targetIDs;
    std::string GetTypeName() const override { return "VistaAndAboveIdListDataBlock"; }
};

struct DamagedDataBlock : ExtraDataBase {
    uint32_t originalSignature = 0;
    std::string errorMessage;
    std::vector<uint8_t> rawBytes;
    std::string GetTypeName() const override { return "DamagedDataBlock"; }
};

} // namespace lecmd
