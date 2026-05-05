#include "XmlWriter.h"
#include <fstream>

namespace lecmd {

bool XmlWriter::Write(const std::string& path, const std::vector<CsvOut>& entries) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<LECmdOutput>\n";
    for (const auto& e : entries) {
        file << "  <LNK>\n";
        file << "    <SourceFile>" << e.sourceFile << "</SourceFile>\n";
        file << "    <SourceCreated>" << e.sourceCreated << "</SourceCreated>\n";
        file << "    <SourceModified>" << e.sourceModified << "</SourceModified>\n";
        file << "    <SourceAccessed>" << e.sourceAccessed << "</SourceAccessed>\n";
        file << "    <TargetCreated>" << e.targetCreated << "</TargetCreated>\n";
        file << "    <TargetModified>" << e.targetModified << "</TargetModified>\n";
        file << "    <TargetAccessed>" << e.targetAccessed << "</TargetAccessed>\n";
        file << "    <FileSize>" << e.fileSize << "</FileSize>\n";
        file << "    <RelativePath>" << e.relativePath << "</RelativePath>\n";
        file << "    <WorkingDirectory>" << e.workingDirectory << "</WorkingDirectory>\n";
        file << "    <FileAttributes>" << e.fileAttributes << "</FileAttributes>\n";
        file << "    <HeaderFlags>" << e.headerFlags << "</HeaderFlags>\n";
        file << "    <DriveType>" << e.driveType << "</DriveType>\n";
        file << "    <VolumeSerialNumber>" << e.volumeSerialNumber << "</VolumeSerialNumber>\n";
        file << "    <VolumeLabel>" << e.volumeLabel << "</VolumeLabel>\n";
        file << "    <LocalPath>" << e.localPath << "</LocalPath>\n";
        file << "    <NetworkPath>" << e.networkPath << "</NetworkPath>\n";
        file << "    <CommonPath>" << e.commonPath << "</CommonPath>\n";
        file << "    <Arguments>" << e.arguments << "</Arguments>\n";
        file << "    <TargetIDAbsolutePath>" << e.targetIDAbsolutePath << "</TargetIDAbsolutePath>\n";
        file << "    <TargetMFTEntryNumber>" << e.targetMFTEntryNumber << "</TargetMFTEntryNumber>\n";
        file << "    <TargetMFTSequenceNumber>" << e.targetMFTSequenceNumber << "</TargetMFTSequenceNumber>\n";
        file << "    <MachineID>" << e.machineID << "</MachineID>\n";
        file << "    <MachineMACAddress>" << e.machineMACAddress << "</MachineMACAddress>\n";
        file << "    <MACVendor>" << e.macVendor << "</MACVendor>\n";
        file << "    <TrackerCreatedOn>" << e.trackerCreatedOn << "</TrackerCreatedOn>\n";
        file << "    <ExtraBlocksPresent>" << e.extraBlocksPresent << "</ExtraBlocksPresent>\n";
        file << "  </LNK>\n";
    }
    file << "</LECmdOutput>\n";
    return true;
}

} // namespace lecmd
