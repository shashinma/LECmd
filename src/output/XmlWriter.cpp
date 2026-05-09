#include "XmlWriter.h"
#include <fstream>

namespace lecmd {

static std::string XmlEscape(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '&': r += "&amp;"; break;
            case '<': r += "&lt;"; break;
            case '>': r += "&gt;"; break;
            case '"': r += "&quot;"; break;
            case '\'': r += "&apos;"; break;
            default: r += c; break;
        }
    }
    return r;
}

static void WriteCsvOutXml(std::ofstream& file, const CsvOut& e) {
    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<CsvOut xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">\n";
    file << "  <SourceFile>" << XmlEscape(e.sourceFile) << "</SourceFile>\n";
    file << "  <SourceCreated>" << XmlEscape(e.sourceCreated) << "</SourceCreated>\n";
    file << "  <SourceModified>" << XmlEscape(e.sourceModified) << "</SourceModified>\n";
    file << "  <SourceAccessed>" << XmlEscape(e.sourceAccessed) << "</SourceAccessed>\n";
    file << "  <TargetCreated>" << XmlEscape(e.targetCreated) << "</TargetCreated>\n";
    file << "  <TargetModified>" << XmlEscape(e.targetModified) << "</TargetModified>\n";
    file << "  <TargetAccessed>" << XmlEscape(e.targetAccessed) << "</TargetAccessed>\n";
    file << "  <FileSize>" << e.fileSize << "</FileSize>\n";
    file << "  <RelativePath>" << XmlEscape(e.relativePath) << "</RelativePath>\n";
    file << "  <WorkingDirectory>" << XmlEscape(e.workingDirectory) << "</WorkingDirectory>\n";
    file << "  <FileAttributes>" << XmlEscape(e.fileAttributes) << "</FileAttributes>\n";
    file << "  <HeaderFlags>" << XmlEscape(e.headerFlags) << "</HeaderFlags>\n";
    file << "  <DriveType>" << XmlEscape(e.driveType) << "</DriveType>\n";
    file << "  <VolumeSerialNumber>" << XmlEscape(e.volumeSerialNumber) << "</VolumeSerialNumber>\n";
    file << "  <VolumeLabel>" << XmlEscape(e.volumeLabel) << "</VolumeLabel>\n";
    file << "  <LocalPath>" << XmlEscape(e.localPath) << "</LocalPath>\n";
    file << "  <NetworkPath>" << XmlEscape(e.networkPath) << "</NetworkPath>\n";
    file << "  <CommonPath>" << XmlEscape(e.commonPath) << "</CommonPath>\n";
    file << "  <Arguments>" << XmlEscape(e.arguments) << "</Arguments>\n";
    file << "  <TargetIDAbsolutePath>" << XmlEscape(e.targetIDAbsolutePath) << "</TargetIDAbsolutePath>\n";
    file << "  <TargetMFTEntryNumber>" << XmlEscape(e.targetMFTEntryNumber) << "</TargetMFTEntryNumber>\n";
    file << "  <TargetMFTSequenceNumber>" << XmlEscape(e.targetMFTSequenceNumber) << "</TargetMFTSequenceNumber>\n";
    file << "  <MachineID>" << XmlEscape(e.machineID) << "</MachineID>\n";
    file << "  <MachineMACAddress>" << XmlEscape(e.machineMACAddress) << "</MachineMACAddress>\n";
    file << "  <MACVendor>" << XmlEscape(e.macVendor) << "</MACVendor>\n";
    file << "  <TrackerCreatedOn>" << XmlEscape(e.trackerCreatedOn) << "</TrackerCreatedOn>\n";
    file << "  <ExtraBlocksPresent>" << XmlEscape(e.extraBlocksPresent) << "</ExtraBlocksPresent>\n";
    file << "</CsvOut>\n";
}

bool XmlWriter::Write(const std::string& path, const std::vector<CsvOut>& entries) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    for (const auto& e : entries) {
        WriteCsvOutXml(file, e);
    }
    return true;
}

bool XmlWriter::WriteSingle(const std::string& path, const CsvOut& entry) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    WriteCsvOutXml(file, entry);
    return true;
}

} // namespace lecmd
