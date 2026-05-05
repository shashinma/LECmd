#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

#include "lnk/LnkFile.h"
#include "output/CsvWriter.h"
#include "output/JsonWriter.h"
#include "output/XmlWriter.h"
#include "output/HtmlWriter.h"
#include "utils/MacVendor.h"
#include "utils/DateTimeUtils.h"

using namespace lecmd;

static std::string g_dateFormat = "%Y-%m-%d %H:%M:%S";
static std::string g_preciseFormat = "%Y-%m-%d %H:%M:%S"; // precise microseconds handled separately

static std::string FormatTime(std::time_t t) {
    if (t == 0) return "";
    return DateTimeUtils::Format(t, g_dateFormat);
}

static std::string FormatFileTime(uint64_t ft) {
    if (ft == 0 || ft == 0xFFFFFFFFFFFFFFFEULL) return "";
    const uint64_t EPOCH_DIFF = 116444736000000000ULL;
    if (ft < EPOCH_DIFF) return "";
    std::time_t t = static_cast<std::time_t>((ft - EPOCH_DIFF) / 10000000);
    return DateTimeUtils::Format(t, g_dateFormat);
}

static std::string GetVersionString() {
    return "LECmd version 1.0.0\n\n"
           "Author: shashinma\n"
           "https://github.com/shashinma\n"
           "https://github.com/Artifactum";
}

static void DumpLnkToConsole(const std::shared_ptr<LnkFile>& lnk, bool nid, bool neb, const MacVendorLookup& macLookup) {
    std::string src = lnk->sourceFile;
    spdlog::info("Source file: {}", src);
    spdlog::info("  Source created:  {}", FormatTime(lnk->sourceCreated.value_or(0)));
    spdlog::info("  Source modified: {}", FormatTime(lnk->sourceModified.value_or(0)));
    spdlog::info("  Source accessed: {}", FormatTime(lnk->sourceAccessed.value_or(0)));
    std::cout << "\n";

    spdlog::info("--- Header ---");
    std::string tc = FormatFileTime(lnk->header.targetCreationTime);
    std::string tm = FormatFileTime(lnk->header.targetModificationTime);
    std::string ta = FormatFileTime(lnk->header.targetAccessTime);
    spdlog::info("  Target created:  {}", tc.empty() ? "" : tc);
    spdlog::info("  Target modified: {}", tm.empty() ? "" : tm);
    spdlog::info("  Target accessed: {}", ta.empty() ? "" : ta);
    std::cout << "\n";
    spdlog::info("  File size (bytes): {}", lnk->header.fileSize);
    spdlog::info("  Flags: {}", lnk->header.GetDataFlagsString());
    spdlog::info("  File attributes: {}", lnk->header.GetFileAttributesString());
    if (!lnk->header.GetHotKeyString().empty()) {
        spdlog::info("  Hot key: {}", lnk->header.GetHotKeyString());
    }
    spdlog::info("  Icon index: {}", lnk->header.iconIndex);
    spdlog::info("  Show window: {} ({})", lnk->header.showWindow, lnk->header.GetShowWindowString());
    std::cout << "\n";

    if (lnk->header.HasFlag(LnkHeader::HasName) && !lnk->name.empty()) {
        spdlog::info("Name: {}", lnk->name);
    }
    if (lnk->header.HasFlag(LnkHeader::HasRelativePath) && !lnk->relativePath.empty()) {
        spdlog::info("Relative Path: {}", lnk->relativePath);
    }
    if (lnk->header.HasFlag(LnkHeader::HasWorkingDir) && !lnk->workingDirectory.empty()) {
        spdlog::info("Working Directory: {}", lnk->workingDirectory);
    }
    if (lnk->header.HasFlag(LnkHeader::HasArguments) && !lnk->arguments.empty()) {
        spdlog::info("Arguments: {}", lnk->arguments);
    }
    if (lnk->header.HasFlag(LnkHeader::HasIconLocation) && !lnk->iconLocation.empty()) {
        spdlog::info("Icon Location: {}", lnk->iconLocation);
    }

    if (lnk->header.HasFlag(LnkHeader::HasLinkInfo) && lnk->linkInfo) {
        std::cout << "\n";
        spdlog::info("--- Link information ---");
        spdlog::info("Flags: {}", lnk->linkInfo->flags);

        if (lnk->linkInfo->volumeInfo) {
            std::cout << "\n";
            spdlog::info(">> Volume information");
            spdlog::info("  Drive type: {}", lnk->linkInfo->volumeInfo->GetDriveTypeString());
            spdlog::info("  Serial number: {}", lnk->linkInfo->volumeInfo->volumeSerialNumber);
            std::string label = lnk->linkInfo->volumeInfo->volumeLabel.empty() ? "(No label)" : lnk->linkInfo->volumeInfo->volumeLabel;
            spdlog::info("  Label: {}", label);
        }

        if (lnk->linkInfo->networkShareInfo) {
            std::cout << "\n";
            spdlog::info("  Network share information");
            if (!lnk->linkInfo->networkShareInfo->deviceName.empty()) {
                spdlog::info("    Device name: {}", lnk->linkInfo->networkShareInfo->deviceName);
            }
            spdlog::info("    Share name: {}", lnk->linkInfo->networkShareInfo->networkShareName);
            spdlog::info("    Provider type: {}", lnk->linkInfo->networkShareInfo->GetProviderTypeString());
            spdlog::info("    Share flags: {}", lnk->linkInfo->networkShareInfo->GetShareFlagsString());
            std::cout << "\n";
        }

        if (!lnk->LocalPath().empty()) {
            spdlog::info("  Local path: {}", lnk->LocalPath());
        }
        if (!lnk->CommonPath().empty()) {
            spdlog::info("  Common path: {}", lnk->CommonPath());
        }
    }

    if (nid) {
        std::cout << "\n";
        spdlog::info("(Target ID information suppressed. Lnk TargetID count: {})", lnk->targetIDs.size());
    }

    if (!lnk->targetIDs.empty() && !nid) {
        std::cout << "\n";
        spdlog::info("--- Target ID information (Format: Type ==> Value) ---");
        std::cout << "\n";
        spdlog::info("  Absolute path: {}", ParseShellBagAbsolutePath(lnk->targetIDs));
        std::cout << "\n";

        for (const auto& bag : lnk->targetIDs) {
            std::string val = bag->value.empty() ? "(None)" : bag->value;
            spdlog::info("  -{} ==> {}", bag->friendlyName, val);
            std::cout << "\n";
        }
        spdlog::info("--- End Target ID information ---");
    }

    if (neb) {
        std::cout << "\n";
        spdlog::info("(Extra blocks information suppressed. Lnk Extra block count: {})", lnk->extraBlocks.size());
    }

    if (!lnk->extraBlocks.empty() && !neb) {
        std::cout << "\n";
        spdlog::info("--- Extra blocks information ---");
        std::cout << "\n";

        for (const auto& eb : lnk->extraBlocks) {
            if (auto tracker = std::dynamic_pointer_cast<TrackerDataBaseBlock>(eb)) {
                spdlog::info(">> Tracker database block");
                spdlog::info("   Machine ID:  {}", tracker->machineId);
                spdlog::info("   MAC Address: {}", tracker->macAddress);
                if (!tracker->macAddress.empty()) {
                    std::string vendor = macLookup.Lookup(tracker->macAddress);
                    if (!vendor.empty()) {
                        spdlog::info("   MAC Vendor:  {}", vendor);
                    }
                }
                if (tracker->creationTime.has_value() && tracker->creationTime.value() != 0) {
                    spdlog::info("   Creation:    {}", FormatTime(tracker->creationTime.value()));
                }
                std::cout << "\n";
                spdlog::info("   Volume Droid:       {}", tracker->volumeDroid);
                spdlog::info("   Volume Droid Birth: {}", tracker->volumeDroidBirth);
                spdlog::info("   File Droid:         {}", tracker->fileDroid);
                spdlog::info("   File Droid birth:   {}", tracker->fileDroidBirth);
                std::cout << "\n";
            } else if (auto console = std::dynamic_pointer_cast<ConsoleDataBlock>(eb)) {
                spdlog::info(">> Console data block");
                spdlog::info("   Fill Attributes: {}", console->fillAttributes);
                spdlog::info("   Is Full Screen: {}", console->fullScreen);
            } else {
                spdlog::info(">> {}", eb->GetTypeName());
            }
        }
    }
}

int main(int argc, char** argv) {
    CLI::App app{"LECmd - LNK Explorer Command Line edition"};
    app.set_version_flag("-v,--version", GetVersionString());

    std::string filePath;
    std::string dirPath;
    bool removableOnly = false;
    bool quiet = false;
    bool allFiles = false;
    std::string csvDir;
    std::string csvf;
    std::string jsonDir;
    std::string xmlDir;
    std::string htmlDir;
    bool pretty = false;
    bool nid = false;
    bool neb = false;
    std::string dt = "%Y-%m-%d %H:%M:%S";
    bool mp = false;
    int cp = 1252;
    bool debug = false;
    bool trace = false;

    app.add_option("-f", filePath, "File to process. Either this or -d is required");
    app.add_option("-d", dirPath, "Directory to recursively process. Either this or -d is required");
    app.add_flag("-r", removableOnly, "Only process lnk files pointing to removable drives");
    app.add_flag("--all", allFiles, "Process all files in directory vs. only files matching *.lnk");
    app.add_option("--csv", csvDir, "Directory to save CSV formatted results to");
    app.add_option("--csvf", csvf, "File name to save CSV formatted results to");
    app.add_option("--json", jsonDir, "Directory to save JSON formatted results to");
    app.add_option("--xml", xmlDir, "Directory to save XML formatted results to");
    app.add_option("--html", htmlDir, "Directory to save HTML formatted results to");
    app.add_flag("--pretty", pretty, "When exporting to json, use a more human readable layout");
    app.add_flag("-q", quiet, "Do not dump full details about each file processed");
    app.add_flag("--nid", nid, "Suppress Target ID list details from being displayed");
    app.add_flag("--neb", neb, "Suppress Extra blocks information from being displayed");
    app.add_option("--dt", dt, "The custom date/time format to use");
    app.add_flag("--mp", mp, "Display higher precision for timestamps");
    app.add_option("--cp", cp, "Code page to parse strings");
    app.add_flag("--debug", debug, "Show debug information during processing");
    app.add_flag("--trace", trace, "Show trace information during processing");

    CLI11_PARSE(app, argc, argv);

    // Setup logging
    if (debug) {
        spdlog::set_level(spdlog::level::debug);
    } else if (trace) {
        spdlog::set_level(spdlog::level::trace);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    g_dateFormat = dt;
    if (mp) {
        g_dateFormat = g_preciseFormat;
    }

    if (filePath.empty() && dirPath.empty()) {
        std::cerr << "Either -f or -d is required. Exiting\n";
        return 1;
    }

    if (!filePath.empty() && !std::filesystem::exists(filePath)) {
        std::cerr << "File " << filePath << " not found. Exiting\n";
        return 1;
    }

    if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
        std::cerr << "Directory " << dirPath << " not found. Exiting\n";
        return 1;
    }

    spdlog::info("{}", GetVersionString());
    std::cout << "\n";
    spdlog::info("Command line: {}", fmt::format("{} {}", argv[0], fmt::join(std::vector<std::string>(argv + 1, argv + argc), " ")));
    std::cout << "\n";

    MacVendorLookup macLookup;
    std::vector<std::shared_ptr<LnkFile>> processedFiles;
    std::vector<std::string> failedFiles;

    auto tsNow = std::chrono::system_clock::now();
    auto tsNowT = std::chrono::system_clock::to_time_t(tsNow);
    std::stringstream tsSs;
    tsSs << std::put_time(std::localtime(&tsNowT), "%Y%m%d%H%M%S");
    std::string tsStr = tsSs.str();

    if (!filePath.empty()) {
        auto lnk = LnkFile::Load(filePath, cp);
        if (lnk) {
            if (!removableOnly || (lnk->linkInfo && lnk->linkInfo->volumeInfo && lnk->linkInfo->volumeInfo->driveType == VolumeInfo::DriveRemovable)) {
                processedFiles.push_back(lnk);
                if (!quiet) {
                    DumpLnkToConsole(lnk, nid, neb, macLookup);
                }
            }
        } else {
            failedFiles.push_back(filePath);
            spdlog::error("Error opening {}", filePath);
        }
    } else {
        spdlog::info("Looking for lnk files in {}", dirPath);
        std::cout << "\n";

        std::vector<std::string> lnkFiles;
        try {
            std::error_code ec;
            std::filesystem::recursive_directory_iterator it(dirPath, std::filesystem::directory_options::skip_permission_denied, ec);
            if (ec) {
                spdlog::error("Error opening directory {}. Error: {}", dirPath, ec.message());
                return 1;
            }
            for (; it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                const auto& entry = *it;
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (allFiles || ext == ".lnk") {
                        lnkFiles.push_back(entry.path().string());
                    }
                }
            }
        } catch (const std::exception& ex) {
            spdlog::error("Error getting lnk files in {}. Error: {}", dirPath, ex.what());
            return 1;
        }

        spdlog::info("Found {} files", lnkFiles.size());
        std::cout << "\n";

        for (const auto& f : lnkFiles) {
            auto lnk = LnkFile::Load(f, cp);
            if (lnk) {
                if (!removableOnly || (lnk->linkInfo && lnk->linkInfo->volumeInfo && lnk->linkInfo->volumeInfo->driveType == VolumeInfo::DriveRemovable)) {
                    processedFiles.push_back(lnk);
                    if (!quiet) {
                        DumpLnkToConsole(lnk, nid, neb, macLookup);
                    }
                }
            } else {
                failedFiles.push_back(f);
                spdlog::error("Error opening {}", f);
            }
        }

        spdlog::info("Processed {} out of {} files", lnkFiles.size() - failedFiles.size(), lnkFiles.size());
        if (!failedFiles.empty()) {
            std::cout << "\n";
            spdlog::info("Failed files");
            for (const auto& ff : failedFiles) {
                spdlog::info("  {}", ff);
            }
        }
    }

    if (processedFiles.empty()) {
        spdlog::info("No files processed");
        return 0;
    }

    // Convert to CsvOut
    std::vector<CsvOut> csvEntries;
    for (const auto& lnk : processedFiles) {
        csvEntries.push_back(GetCsvFormat(lnk, g_dateFormat, &macLookup));
    }

    // CSV output
    if (!csvDir.empty()) {
        if (!std::filesystem::exists(csvDir)) {
            std::filesystem::create_directories(csvDir);
        }
        std::string outName = tsStr + "_LECmd_Output.csv";
        if (!csvf.empty()) {
            outName = csvf;
        }
        std::string outFile = (std::filesystem::path(csvDir) / outName).string();
        spdlog::info("CSV output will be saved to {}", outFile);
        CsvWriter csvWriter;
        if (!csvWriter.Write(outFile, csvEntries)) {
            spdlog::error("Failed to write CSV output");
        }
    }

    // JSON output
    if (!jsonDir.empty()) {
        if (!std::filesystem::exists(jsonDir)) {
            std::filesystem::create_directories(jsonDir);
        }
        std::string outName = tsStr + "_LECmd_Output.json";
        std::string outFile = (std::filesystem::path(jsonDir) / outName).string();
        spdlog::info("Saving json output to {}", jsonDir);
        JsonWriter jsonWriter;
        if (!jsonWriter.Write(outFile, csvEntries, pretty)) {
            spdlog::error("Failed to write JSON output");
        }
    }

    // XML output
    if (!xmlDir.empty()) {
        if (!std::filesystem::exists(xmlDir)) {
            std::filesystem::create_directories(xmlDir);
        }
        std::string outName = tsStr + "_LECmd_Output.xml";
        std::string outFile = (std::filesystem::path(xmlDir) / outName).string();
        spdlog::info("Saving XML output to {}", xmlDir);
        XmlWriter xmlWriter;
        if (!xmlWriter.Write(outFile, csvEntries)) {
            spdlog::error("Failed to write XML output");
        }
    }

    // HTML output
    if (!htmlDir.empty()) {
        if (!std::filesystem::exists(htmlDir)) {
            std::filesystem::create_directories(htmlDir);
        }
        std::string outName = tsStr + "_LECmd_Output.html";
        std::string outFile = (std::filesystem::path(htmlDir) / outName).string();
        spdlog::info("Saving HTML output to {}", htmlDir);
        HtmlWriter htmlWriter;
        if (!htmlWriter.Write(outFile, csvEntries)) {
            spdlog::error("Failed to write HTML output");
        }
    }

    std::cout << "\n";
    spdlog::info("Results saved.");

    return 0;
}
