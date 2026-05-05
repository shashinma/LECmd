#include "HtmlWriter.h"
#include <fstream>

namespace lecmd {

bool HtmlWriter::Write(const std::string& path, const std::vector<CsvOut>& entries) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "<!DOCTYPE html>\n<html>\n<head>\n"
         << "<meta charset=\"UTF-8\">\n"
         << "<title>LECmd Output</title>\n"
         << "<style>\n"
         << "body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5}\n"
         << "table{border-collapse:collapse;width:100%;background:#fff;font-size:12px}\n"
         << "th,td{border:1px solid #ddd;padding:6px;text-align:left}\n"
         << "th{background:#4CAF50;color:white}\n"
         << "tr:nth-child(even){background:#f2f2f2}\n"
         << "</style>\n"
         << "</head>\n<body>\n"
         << "<h1>LECmd Output</h1>\n"
         << "<table>\n<tr>"
         << "<th>SourceFile</th><th>SourceCreated</th><th>SourceModified</th><th>SourceAccessed</th>"
         << "<th>TargetCreated</th><th>TargetModified</th><th>TargetAccessed</th><th>FileSize</th>"
         << "<th>RelativePath</th><th>WorkingDirectory</th><th>FileAttributes</th><th>HeaderFlags</th>"
         << "<th>DriveType</th><th>VolumeSerialNumber</th><th>VolumeLabel</th><th>LocalPath</th>"
         << "<th>NetworkPath</th><th>CommonPath</th><th>Arguments</th><th>TargetIDAbsolutePath</th>"
         << "<th>TargetMFTEntryNumber</th><th>TargetMFTSequenceNumber</th><th>MachineID</th>"
         << "<th>MachineMACAddress</th><th>MACVendor</th><th>TrackerCreatedOn</th><th>ExtraBlocksPresent</th>"
         << "</tr>\n";

    for (const auto& e : entries) {
        auto esc = [](const std::string& s) -> std::string {
            std::string r;
            for (char c : s) {
                switch (c) {
                    case '&': r += "&amp;"; break;
                    case '<': r += "&lt;"; break;
                    case '>': r += "&gt;"; break;
                    case '"': r += "&quot;"; break;
                    default: r += c; break;
                }
            }
            return r;
        };
        file << "<tr>"
             << "<td>" << esc(e.sourceFile) << "</td>"
             << "<td>" << esc(e.sourceCreated) << "</td>"
             << "<td>" << esc(e.sourceModified) << "</td>"
             << "<td>" << esc(e.sourceAccessed) << "</td>"
             << "<td>" << esc(e.targetCreated) << "</td>"
             << "<td>" << esc(e.targetModified) << "</td>"
             << "<td>" << esc(e.targetAccessed) << "</td>"
             << "<td>" << e.fileSize << "</td>"
             << "<td>" << esc(e.relativePath) << "</td>"
             << "<td>" << esc(e.workingDirectory) << "</td>"
             << "<td>" << esc(e.fileAttributes) << "</td>"
             << "<td>" << esc(e.headerFlags) << "</td>"
             << "<td>" << esc(e.driveType) << "</td>"
             << "<td>" << esc(e.volumeSerialNumber) << "</td>"
             << "<td>" << esc(e.volumeLabel) << "</td>"
             << "<td>" << esc(e.localPath) << "</td>"
             << "<td>" << esc(e.networkPath) << "</td>"
             << "<td>" << esc(e.commonPath) << "</td>"
             << "<td>" << esc(e.arguments) << "</td>"
             << "<td>" << esc(e.targetIDAbsolutePath) << "</td>"
             << "<td>" << esc(e.targetMFTEntryNumber) << "</td>"
             << "<td>" << esc(e.targetMFTSequenceNumber) << "</td>"
             << "<td>" << esc(e.machineID) << "</td>"
             << "<td>" << esc(e.machineMACAddress) << "</td>"
             << "<td>" << esc(e.macVendor) << "</td>"
             << "<td>" << esc(e.trackerCreatedOn) << "</td>"
             << "<td>" << esc(e.extraBlocksPresent) << "</td>"
             << "</tr>\n";
    }

    file << "</table>\n</body>\n</html>\n";
    return true;
}

} // namespace lecmd
