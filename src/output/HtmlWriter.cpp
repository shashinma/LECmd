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
         << "table{border-collapse:collapse;width:100%;background:#fff}\n"
         << "th,td{border:1px solid #ddd;padding:8px;text-align:left;font-size:12px}\n"
         << "th{background:#4CAF50;color:white}\n"
         << "tr:nth-child(even){background:#f2f2f2}\n"
         << "</style>\n"
         << "</head>\n<body>\n"
         << "<h1>LECmd Output</h1>\n"
         << "<table>\n<tr>"
         << "<th>SourceFile</th><th>SourceCreated</th><th>SourceModified</th>"
         << "<th>TargetCreated</th><th>TargetModified</th><th>FileSize</th>"
         << "<th>RelativePath</th><th>WorkingDirectory</th><th>LocalPath</th>"
         << "<th>Arguments</th><th>MachineID</th><th>MACAddress</th>"
         << "</tr>\n";

    for (const auto& e : entries) {
        file << "<tr>"
             << "<td>" << e.sourceFile << "</td>"
             << "<td>" << e.sourceCreated << "</td>"
             << "<td>" << e.sourceModified << "</td>"
             << "<td>" << e.targetCreated << "</td>"
             << "<td>" << e.targetModified << "</td>"
             << "<td>" << e.fileSize << "</td>"
             << "<td>" << e.relativePath << "</td>"
             << "<td>" << e.workingDirectory << "</td>"
             << "<td>" << e.localPath << "</td>"
             << "<td>" << e.arguments << "</td>"
             << "<td>" << e.machineID << "</td>"
             << "<td>" << e.machineMACAddress << "</td>"
             << "</tr>\n";
    }

    file << "</table>\n</body>\n</html>\n";
    return true;
}

} // namespace lecmd
