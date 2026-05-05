#include "JsonWriter.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace lecmd {

bool JsonWriter::Write(const std::string& path, const std::vector<CsvOut>& entries, bool pretty) {
    json root = json::array();
    for (const auto& e : entries) {
        json j;
        j["SourceFile"] = e.sourceFile;
        j["SourceCreated"] = e.sourceCreated;
        j["SourceModified"] = e.sourceModified;
        j["SourceAccessed"] = e.sourceAccessed;
        j["TargetCreated"] = e.targetCreated;
        j["TargetModified"] = e.targetModified;
        j["TargetAccessed"] = e.targetAccessed;
        j["FileSize"] = e.fileSize;
        j["RelativePath"] = e.relativePath;
        j["WorkingDirectory"] = e.workingDirectory;
        j["FileAttributes"] = e.fileAttributes;
        j["HeaderFlags"] = e.headerFlags;
        j["DriveType"] = e.driveType;
        j["VolumeSerialNumber"] = e.volumeSerialNumber;
        j["VolumeLabel"] = e.volumeLabel;
        j["LocalPath"] = e.localPath;
        j["NetworkPath"] = e.networkPath;
        j["CommonPath"] = e.commonPath;
        j["Arguments"] = e.arguments;
        j["TargetIDAbsolutePath"] = e.targetIDAbsolutePath;
        j["TargetMFTEntryNumber"] = e.targetMFTEntryNumber;
        j["TargetMFTSequenceNumber"] = e.targetMFTSequenceNumber;
        j["MachineID"] = e.machineID;
        j["MachineMACAddress"] = e.machineMACAddress;
        j["MACVendor"] = e.macVendor;
        j["TrackerCreatedOn"] = e.trackerCreatedOn;
        j["ExtraBlocksPresent"] = e.extraBlocksPresent;
        root.push_back(j);
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;

    if (pretty) {
        file << root.dump(2);
    } else {
        file << root.dump();
    }
    return true;
}

} // namespace lecmd
