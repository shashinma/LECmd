#pragma once

#include "Header.h"
#include "VolumeInfo.h"
#include "LinkInfo.h"
#include "ShellItems.h"
#include "ExtraData.h"

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace lecmd {

class LnkFile {
public:
    static std::shared_ptr<LnkFile> Load(const std::string& path, int codepage = 1252);

    std::string sourceFile;
    std::optional<std::time_t> sourceCreated;
    std::optional<std::time_t> sourceModified;
    std::optional<std::time_t> sourceAccessed;

    LnkHeader header;

    // String data
    std::string name;
    std::string relativePath;
    std::string workingDirectory;
    std::string arguments;
    std::string iconLocation;

    // Link info
    std::shared_ptr<LinkInfo> linkInfo;

    // Target IDs (Shell Items)
    std::vector<std::shared_ptr<ShellBag>> targetIDs;

    // Extra data blocks
    std::vector<std::shared_ptr<ExtraDataBase>> extraBlocks;

    // Convenience accessors
    std::string LocalPath() const;
    std::string CommonPath() const;
    bool HasTrackerData() const;
    std::shared_ptr<TrackerDataBaseBlock> GetTrackerData() const;

private:
    int codepage_ = 1252;
    bool Parse(const std::vector<uint8_t>& raw, int codepage);
    void ParseStringData(const std::vector<uint8_t>& raw, size_t& index);
    void ParseLinkInfo(const std::vector<uint8_t>& raw, size_t& index);
    void ParseTargetIDList(const std::vector<uint8_t>& raw, size_t& index);
    void ParseExtraData(const std::vector<uint8_t>& raw, size_t& index);

    std::shared_ptr<ShellBag> ParseShellItem(const std::vector<uint8_t>& raw, int codepage);
    void ParseExtensionBlocks(ShellBag* bag, const std::vector<uint8_t>& raw, size_t offset, size_t maxLen, int codepage);
    std::shared_ptr<ExtensionBlock> ParseExtensionBlock(const std::vector<uint8_t>& raw, size_t& offset, int codepage);
};

} // namespace lecmd
