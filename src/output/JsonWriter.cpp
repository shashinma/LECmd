#include "JsonWriter.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace lecmd {

static json ToJson(const CsvOut& e, bool nukeNulls) {
    auto setField = [&](json& j, const char* key, const std::string& value) {
        if (nukeNulls && value.empty()) {
            j[key] = nullptr;
        } else {
            j[key] = value;
        }
    };

    json j;
    setField(j, "SourceFile", e.sourceFile);
    setField(j, "SourceCreated", e.sourceCreated);
    setField(j, "SourceModified", e.sourceModified);
    setField(j, "SourceAccessed", e.sourceAccessed);
    setField(j, "TargetCreated", e.targetCreated);
    setField(j, "TargetModified", e.targetModified);
    setField(j, "TargetAccessed", e.targetAccessed);
    j["FileSize"] = e.fileSize;
    setField(j, "RelativePath", e.relativePath);
    setField(j, "WorkingDirectory", e.workingDirectory);
    setField(j, "FileAttributes", e.fileAttributes);
    setField(j, "HeaderFlags", e.headerFlags);
    setField(j, "DriveType", e.driveType);
    setField(j, "VolumeSerialNumber", e.volumeSerialNumber);
    setField(j, "VolumeLabel", e.volumeLabel);
    setField(j, "LocalPath", e.localPath);
    setField(j, "NetworkPath", e.networkPath);
    setField(j, "CommonPath", e.commonPath);
    setField(j, "Arguments", e.arguments);
    setField(j, "TargetIDAbsolutePath", e.targetIDAbsolutePath);
    setField(j, "TargetMFTEntryNumber", e.targetMFTEntryNumber);
    setField(j, "TargetMFTSequenceNumber", e.targetMFTSequenceNumber);
    setField(j, "MachineID", e.machineID);
    setField(j, "MachineMACAddress", e.machineMACAddress);
    setField(j, "MACVendor", e.macVendor);
    setField(j, "TrackerCreatedOn", e.trackerCreatedOn);
    setField(j, "ExtraBlocksPresent", e.extraBlocksPresent);
    return j;
}

bool JsonWriter::Write(const std::string& path, const std::vector<CsvOut>& entries, bool pretty) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    for (const auto& e : entries) {
        json j = ToJson(e, true); // nukeNulls = true for JSON
        if (pretty) {
            file << j.dump(2);
        } else {
            file << j.dump();
        }
        file << "\n";
    }
    return true;
}

} // namespace lecmd
