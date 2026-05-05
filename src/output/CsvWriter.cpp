#include "CsvWriter.h"
#include "../utils/MacVendor.h"
#include "../utils/DateTimeUtils.h"
#include <fstream>
#include <sstream>

namespace lecmd {

std::string CsvWriter::EscapeField(const std::string& field) {
    if (field.find_first_of(",\"\n\r") == std::string::npos) {
        return field;
    }
    std::string result = "\"";
    for (char c : field) {
        if (c == '"') result += "\"\"";
        else result += c;
    }
    result += "\"";
    return result;
}

bool CsvWriter::Write(const std::string& path, const std::vector<CsvOut>& entries) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "SourceFile,SourceCreated,SourceModified,SourceAccessed,"
         << "TargetCreated,TargetModified,TargetAccessed,FileSize,"
         << "RelativePath,WorkingDirectory,FileAttributes,HeaderFlags,"
         << "DriveType,VolumeSerialNumber,VolumeLabel,LocalPath,NetworkPath,"
         << "CommonPath,Arguments,TargetIDAbsolutePath,TargetMFTEntryNumber,"
         << "TargetMFTSequenceNumber,MachineID,MachineMACAddress,MACVendor,"
         << "TrackerCreatedOn,ExtraBlocksPresent\n";

    for (const auto& e : entries) {
        file << EscapeField(e.sourceFile) << ","
             << EscapeField(e.sourceCreated) << ","
             << EscapeField(e.sourceModified) << ","
             << EscapeField(e.sourceAccessed) << ","
             << EscapeField(e.targetCreated) << ","
             << EscapeField(e.targetModified) << ","
             << EscapeField(e.targetAccessed) << ","
             << e.fileSize << ","
             << EscapeField(e.relativePath) << ","
             << EscapeField(e.workingDirectory) << ","
             << EscapeField(e.fileAttributes) << ","
             << EscapeField(e.headerFlags) << ","
             << EscapeField(e.driveType) << ","
             << EscapeField(e.volumeSerialNumber) << ","
             << EscapeField(e.volumeLabel) << ","
             << EscapeField(e.localPath) << ","
             << EscapeField(e.networkPath) << ","
             << EscapeField(e.commonPath) << ","
             << EscapeField(e.arguments) << ","
             << EscapeField(e.targetIDAbsolutePath) << ","
             << EscapeField(e.targetMFTEntryNumber) << ","
             << EscapeField(e.targetMFTSequenceNumber) << ","
             << EscapeField(e.machineID) << ","
             << EscapeField(e.machineMACAddress) << ","
             << EscapeField(e.macVendor) << ","
             << EscapeField(e.trackerCreatedOn) << ","
             << EscapeField(e.extraBlocksPresent) << "\n";
    }

    return true;
}

static std::string FormatTime(std::time_t t, const std::string& fmt) {
    if (t == 0) return "";
    return DateTimeUtils::Format(t, fmt);
}

static std::string FormatFileTime(uint64_t ft, const std::string& fmt) {
    if (ft == 0 || ft == 0xFFFFFFFFFFFFFFFEULL) return "";
    // FILETIME to time_t
    const uint64_t EPOCH_DIFF = 116444736000000000ULL;
    if (ft < EPOCH_DIFF) return "";
    std::time_t t = static_cast<std::time_t>((ft - EPOCH_DIFF) / 10000000);
    return DateTimeUtils::Format(t, fmt);
}

CsvOut GetCsvFormat(const std::shared_ptr<LnkFile>& lnk, const std::string& dateFormat, MacVendorLookup* macLookup) {
    CsvOut out;
    if (!lnk) return out;

    out.sourceFile = lnk->sourceFile;
    out.sourceCreated = FormatTime(lnk->sourceCreated.value_or(0), dateFormat);
    out.sourceModified = FormatTime(lnk->sourceModified.value_or(0), dateFormat);
    out.sourceAccessed = FormatTime(lnk->sourceAccessed.value_or(0), dateFormat);
    out.targetCreated = FormatFileTime(lnk->header.targetCreationTime, dateFormat);
    out.targetModified = FormatFileTime(lnk->header.targetModificationTime, dateFormat);
    out.targetAccessed = FormatFileTime(lnk->header.targetAccessTime, dateFormat);
    out.fileSize = lnk->header.fileSize;
    out.relativePath = lnk->relativePath;
    out.workingDirectory = lnk->workingDirectory;
    out.fileAttributes = lnk->header.GetFileAttributesString();
    out.headerFlags = lnk->header.GetDataFlagsString();
    out.localPath = lnk->LocalPath();
    out.commonPath = lnk->CommonPath();
    out.arguments = lnk->arguments;

    if (lnk->linkInfo && lnk->linkInfo->volumeInfo) {
        out.driveType = lnk->linkInfo->volumeInfo->GetDriveTypeString();
        out.volumeSerialNumber = lnk->linkInfo->volumeInfo->volumeSerialNumber;
        out.volumeLabel = lnk->linkInfo->volumeInfo->volumeLabel;
    } else {
        out.driveType = "(None)";
    }

    if (lnk->linkInfo && lnk->linkInfo->networkShareInfo) {
        out.networkPath = lnk->linkInfo->networkShareInfo->networkShareName;
    }

    out.targetIDAbsolutePath = ParseShellBagAbsolutePath(lnk->targetIDs);

    // Extract MFT from Beef0004 in last target ID
    if (!lnk->targetIDs.empty()) {
        const auto& lastBag = lnk->targetIDs.back();
        for (const auto& eb : lastBag->extensionBlocks) {
            if (auto b4 = std::dynamic_pointer_cast<Beef0004Block>(eb)) {
                if (b4->mftInfo.mftEntryNumber.has_value()) {
                    std::ostringstream oss;
                    oss << "0x" << std::hex << *b4->mftInfo.mftEntryNumber;
                    out.targetMFTEntryNumber = oss.str();
                }
                if (b4->mftInfo.mftSequenceNumber.has_value()) {
                    std::ostringstream oss;
                    oss << "0x" << std::hex << *b4->mftInfo.mftSequenceNumber;
                    out.targetMFTSequenceNumber = oss.str();
                }
            }
        }
    }

    // Extra blocks present
    std::vector<std::string> ebNames;
    for (const auto& eb : lnk->extraBlocks) {
        ebNames.push_back(eb->GetTypeName());
    }
    std::ostringstream ebOss;
    for (size_t i = 0; i < ebNames.size(); ++i) {
        if (i > 0) ebOss << ", ";
        ebOss << ebNames[i];
    }
    out.extraBlocksPresent = ebOss.str();

    // Tracker data
    auto tracker = lnk->GetTrackerData();
    if (tracker) {
        out.machineID = tracker->machineId;
        out.machineMACAddress = tracker->macAddress;
        if (macLookup) {
            out.macVendor = macLookup->Lookup(tracker->macAddress);
        }
        out.trackerCreatedOn = FormatTime(tracker->creationTime.value_or(0), dateFormat);
    }

    return out;
}

} // namespace lecmd
