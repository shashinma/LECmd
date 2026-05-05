#pragma once

#include "../lnk/LnkFile.h"
#include <string>
#include <vector>

namespace lecmd {

struct CsvOut {
    std::string sourceFile;
    std::string sourceCreated;
    std::string sourceModified;
    std::string sourceAccessed;
    std::string targetCreated;
    std::string targetModified;
    std::string targetAccessed;
    uint32_t fileSize = 0;
    std::string relativePath;
    std::string workingDirectory;
    std::string fileAttributes;
    std::string headerFlags;
    std::string driveType;
    std::string volumeSerialNumber;
    std::string volumeLabel;
    std::string localPath;
    std::string networkPath;
    std::string commonPath;
    std::string arguments;
    std::string targetIDAbsolutePath;
    std::string targetMFTEntryNumber;
    std::string targetMFTSequenceNumber;
    std::string machineID;
    std::string machineMACAddress;
    std::string macVendor;
    std::string trackerCreatedOn;
    std::string extraBlocksPresent;
};

class CsvWriter {
public:
    bool Write(const std::string& path, const std::vector<CsvOut>& entries);
private:
    std::string EscapeField(const std::string& field);
};

CsvOut GetCsvFormat(const std::shared_ptr<LnkFile>& lnk, const std::string& dateFormat, class MacVendorLookup* macLookup);

} // namespace lecmd
