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
#include "lnk/PropertyStore.h"
#include "output/CsvWriter.h"
#include "output/JsonWriter.h"
#include "output/XmlWriter.h"
#include "output/HtmlWriter.h"
#include "utils/MacVendor.h"
#include "utils/DateTimeUtils.h"

using namespace lecmd;

static std::string g_dateFormat = "%Y-%m-%d %H:%M:%S";
static bool g_useMicroseconds = false;

static std::string FormatTime(std::time_t t) {
    if (t == 0) return "";
    if (g_useMicroseconds) {
        return DateTimeUtils::FormatMicroseconds(t);
    }
    return DateTimeUtils::Format(t, g_dateFormat);
}

static std::string FormatFileSize(uint32_t size) {
    std::string s = std::to_string(size);
    std::string result;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i > 0 && (s.size() - i) % 3 == 0) result += ',';
        result += s[i];
    }
    return result;
}

static std::string FormatFileTime(uint64_t ft) {
    if (ft == 0 || ft == 0xFFFFFFFFFFFFFFFEULL) return "";
    const uint64_t EPOCH_DIFF = 116444736000000000ULL;
    if (ft < EPOCH_DIFF) return "";
    std::time_t t = static_cast<std::time_t>((ft - EPOCH_DIFF) / 10000000);
    if (g_useMicroseconds) {
        return DateTimeUtils::FormatMicroseconds(t);
    }
    return DateTimeUtils::Format(t, g_dateFormat);
}

static std::string GetVersionString() {
    return "LECmd version 1.0.0\n\n"
           "Author: shashinma\n"
           "https://github.com/shashinma\n"
           "https://github.com/Artifactum";
}

static void DumpExtensionBlockDetails(const std::shared_ptr<ExtensionBlock>& eb, int blockNum) {
    spdlog::info("    --------- Block {} ({}) ---------", blockNum, eb->GetTypeName());
    if (auto b4 = std::dynamic_pointer_cast<Beef0004Block>(eb)) {
        spdlog::info("    Long name: {}", b4->longName);
        if (!b4->localisedName.empty()) {
            spdlog::info("    Localized name: {}", b4->localisedName);
        }
        if (b4->createdOnTime.has_value() && b4->createdOnTime.value() != 0) {
            spdlog::info("    Created:     {}", DateTimeUtils::Format(b4->createdOnTime.value(), "%Y-%m-%d %H:%M:%S"));
        } else {
            spdlog::info("    Created:");
        }
        if (b4->lastAccessTime.has_value() && b4->lastAccessTime.value() != 0) {
            spdlog::info("    Last access: {}", DateTimeUtils::Format(b4->lastAccessTime.value(), "%Y-%m-%d %H:%M:%S"));
        } else {
            spdlog::info("    Last access: ");
        }
        if (b4->mftInfo.mftEntryNumber.has_value() && b4->mftInfo.mftEntryNumber.value() > 0) {
            spdlog::info("    MFT entry/sequence #: {}/{} (0x{:X}/0x{:X})",
                b4->mftInfo.mftEntryNumber.value(),
                b4->mftInfo.mftSequenceNumber.value_or(0),
                b4->mftInfo.mftEntryNumber.value(),
                b4->mftInfo.mftSequenceNumber.value_or(0));
        }
        spdlog::info("    Identifier: 0x{:X} ({})", b4->identifier, b4->GetOsHint());
        if (!b4->mftInfo.note.empty()) {
            spdlog::info("    File system hint: {}", b4->mftInfo.note);
        }
    } else if (auto b25 = std::dynamic_pointer_cast<Beef0025Block>(eb)) {
        std::string ft1 = b25->fileTime1.has_value() ? DateTimeUtils::Format(b25->fileTime1.value(), "%Y-%m-%d %H:%M:%S") : "";
        std::string ft2 = b25->fileTime2.has_value() ? DateTimeUtils::Format(b25->fileTime2.value(), "%Y-%m-%d %H:%M:%S") : "";
        spdlog::info("    Filetime 1: {}, Filetime 2: {}", ft1, ft2);
    } else if (auto b3 = std::dynamic_pointer_cast<Beef0003Block>(eb)) {
        if (!b3->guid1Folder.empty()) {
            spdlog::info("    GUID: {} ({})", b3->guid1, b3->guid1Folder);
        } else {
            spdlog::info("    GUID: {}", b3->guid1);
        }
    } else if (auto b1a = std::dynamic_pointer_cast<Beef001aBlock>(eb)) {
        spdlog::info("    File document type: {}", b1a->fileDocumentTypeString);
    } else {
        spdlog::info("    {}", eb->GetTypeName());
    }
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
    spdlog::info("  File size (bytes): {}", FormatFileSize(lnk->header.fileSize));
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
        spdlog::info("Flags: {}", lnk->linkInfo->GetFlagsString());

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

            // ShellBag0x00 details
            if (bag->type == 0x00) {
                if (!bag->propertyStore.empty()) {
                    spdlog::info("  >> Property store (Format: GUID\\ID Description ==> Value)");
                    for (const auto& kv : bag->propertyStore) {
                        spdlog::info("   {} ==> {}", kv.first, kv.second);
                    }
                }
                if (bag->createdOnTime.has_value() && bag->createdOnTime.value() != 0) {
                    spdlog::info("    Created On: {}", DateTimeUtils::Format(bag->createdOnTime.value(), "%Y-%m-%d %H:%M:%S"));
                }
                if (bag->lastModificationTime.has_value() && bag->lastModificationTime.value() != 0) {
                    spdlog::info("    Modified On: {}", DateTimeUtils::Format(bag->lastModificationTime.value(), "%Y-%m-%d %H:%M:%S"));
                }
                if (bag->lastAccessTime.has_value() && bag->lastAccessTime.value() != 0) {
                    spdlog::info("    Accessed On: {}", DateTimeUtils::Format(bag->lastAccessTime.value(), "%Y-%m-%d %H:%M:%S"));
                }
                if (!bag->fullUrl.empty()) {
                    spdlog::info("    Full URL: {}", bag->fullUrl);
                }
                if (bag->ftpFolderTime.has_value() && bag->ftpFolderTime.value() != 0) {
                    spdlog::info("    FTP folder time: {}", DateTimeUtils::Format(bag->ftpFolderTime.value(), "%Y-%m-%d %H:%M:%S"));
                }
                if (!bag->shortName.empty()) {
                    spdlog::info("    Short name: {}", bag->shortName);
                }
                if (!bag->mtpGuids.empty()) {
                    spdlog::info("    MTP GUIDs");
                    for (const auto& g : bag->mtpGuids) {
                        spdlog::info("    {}", g);
                    }
                }
                if (!bag->fileSystemName.empty()) {
                    spdlog::info("    File system name: {}", bag->fileSystemName);
                }
                if (!bag->storageIdName.empty()) {
                    spdlog::info("    Storage ID name: {}", bag->storageIdName);
                }
                if (!bag->classId.empty()) {
                    spdlog::info("    Class ID: {}", bag->classId);
                }
                if (!bag->mtpType1GuidName.empty()) {
                    spdlog::info("    GUID: {}", bag->mtpType1GuidName);
                }
                if (!bag->extensionBlocks.empty()) {
                    std::cout << "\n";
                    int extNum = 0;
                    for (const auto& eb : bag->extensionBlocks) {
                        DumpExtensionBlockDetails(eb, extNum);
                        extNum++;
                    }
                }
                std::cout << "\n";
                continue;
            }

            // ShellBag0x1F details
            if (bag->type == 0x1f) {
                if (!bag->propertyStore.empty()) {
                    spdlog::info("  >> Property store (Format: GUID\\ID Description ==> Value)");
                    for (const auto& kv : bag->propertyStore) {
                        spdlog::info("   {} ==> {}", kv.first, kv.second);
                    }
                }
                if (bag->lastAccessTime.has_value() && bag->lastAccessTime.value() != 0) {
                    spdlog::info("    Accessed On: {}", DateTimeUtils::Format(bag->lastAccessTime.value(), "%Y-%m-%d %H:%M:%S"));
                }
                if (bag->modifiedDateFromBackup.has_value() && bag->modifiedDateFromBackup.value() != 0) {
                    spdlog::info("    Modified Date From Backup: {}", DateTimeUtils::Format(bag->modifiedDateFromBackup.value(), "%Y-%m-%d %H:%M:%S"));
                }
                if (bag->createdDateFromBackup.has_value() && bag->createdDateFromBackup.value() != 0) {
                    spdlog::info("    Created Date From Backup: {}", DateTimeUtils::Format(bag->createdDateFromBackup.value(), "%Y-%m-%d %H:%M:%S"));
                }
                if (bag->backupDateTime.has_value() && bag->backupDateTime.value() != 0) {
                    spdlog::info("    Backup Date Time: {}", DateTimeUtils::Format(bag->backupDateTime.value(), "%Y-%m-%d %H:%M:%S"));
                }
                if (bag->backupUnknownDateTime.has_value() && bag->backupUnknownDateTime.value() != 0) {
                    spdlog::info("    Backup Unknown Date Time: {}", DateTimeUtils::Format(bag->backupUnknownDateTime.value(), "%Y-%m-%d %H:%M:%S"));
                }
                if (!bag->extensionBlocks.empty()) {
                    std::cout << "\n";
                    int extNum = 0;
                    for (const auto& eb : bag->extensionBlocks) {
                        DumpExtensionBlockDetails(eb, extNum);
                        extNum++;
                    }
                }
                std::cout << "\n";
                continue;
            }

            // ShellBag0x71 details
            if (bag->type == 0x71) {
                if (!bag->propertyStore.empty()) {
                    spdlog::critical("Property stores found! Please email lnk file to saericzimmerman@gmail.com so support can be added!!");
                }
                std::cout << "\n";
                continue;
            }

            // ShellBag0x01 details
            if (bag->type == 0x01) {
                if (!bag->driveLetter.empty()) {
                    spdlog::info("    Drive letter: {}", bag->driveLetter);
                }
                std::cout << "\n";
                continue;
            }

            // Drive letter types (0x22, 0x23, 0x2a, 0x2f) — no extra details
            if (bag->type == 0x22 || bag->type == 0x23 || bag->type == 0x2a || bag->type == 0x2f) {
                std::cout << "\n";
                continue;
            }

            // ShellBag0x2E details
            if (bag->type == 0x2e) {
                if (!bag->propertyStore.empty()) {
                    spdlog::info("  >> Property store (Format: GUID\\ID Description ==> Value)");
                    for (const auto& kv : bag->propertyStore) {
                        spdlog::info("   {} ==> {}", kv.first, kv.second);
                    }
                }
                if (bag->lastAccessTime.has_value() && bag->lastAccessTime.value() != 0) {
                    spdlog::info("    Accessed On: {}", DateTimeUtils::Format(bag->lastAccessTime.value(), "%Y-%m-%d %H:%M:%S"));
                }
                if (!bag->devicePath.empty()) {
                    spdlog::info("    Device path: {}", bag->devicePath);
                }
                if (!bag->category.empty()) {
                    spdlog::info("    Category: {}", bag->category);
                }
                if (!bag->extensionBlocks.empty()) {
                    std::cout << "\n";
                    int extNum = 0;
                    for (const auto& eb : bag->extensionBlocks) {
                        DumpExtensionBlockDetails(eb, extNum);
                        extNum++;
                    }
                }
                std::cout << "\n";
                continue;
            }

            // ShellBag0x40 details
            if (bag->type == 0x40 || bag->type == 0x41 || bag->type == 0x42 || bag->type == 0x43 ||
                bag->type == 0x46 || bag->type == 0x47 || bag->type == 0x49 || bag->type == 0x4a || bag->type == 0x4b) {
                if (!bag->networkDesc.empty()) {
                    spdlog::info("    Description: {}", bag->networkDesc);
                }
                std::cout << "\n";
                continue;
            }

            // ShellBag0x61 details
            if (bag->type == 0x61) {
                if (!bag->uri.empty()) {
                    spdlog::info("    URI: {}", bag->uri);
                }
                if (bag->fileTime1.has_value() && bag->fileTime1.value() != 0) {
                    spdlog::info("    Connect time: {}", DateTimeUtils::Format(bag->fileTime1.value(), "%Y-%m-%d %H:%M:%S"));
                }
                if (!bag->userName.empty()) {
                    spdlog::info("    Username: {}", bag->userName);
                }
                std::cout << "\n";
                continue;
            }

            // ShellBag0x4C details
            if (bag->type == 0x4c) {
                std::cout << "\n";
                continue;
            }

            // ShellBag0xC3 details
            if (bag->type == 0xc3) {
                spdlog::info("    Class type: 0x{:X}", bag->c3ClassType);
                spdlog::info("    Flags: 0x{:X}", bag->c3Flags);
                std::cout << "\n";
                continue;
            }

            // Detailed output for directory/file/delegate items with extension blocks
            bool hasDetails = (bag->type == 0x31 || bag->type == 0x32 || bag->type == 0x36 || bag->type == 0x74);
            if (hasDetails) {
                spdlog::info("    Short name: {}", bag->shortName.empty() ? "" : bag->shortName);
                if (bag->lastModificationTime.has_value() && bag->lastModificationTime.value() != 0) {
                    spdlog::info("    Modified:    {}", DateTimeUtils::Format(bag->lastModificationTime.value(), "%Y-%m-%d %H:%M:%S"));
                } else {
                    spdlog::info("    Modified:");
                }

                if (!bag->extensionBlocks.empty()) {
                    spdlog::info("    Extension block count: {}", bag->extensionBlocks.size());
                    std::cout << "\n";
                    int extNum = 0;
                    for (const auto& eb : bag->extensionBlocks) {
                        DumpExtensionBlockDetails(eb, extNum);
                        extNum++;
                    }
                }
                std::cout << "\n";
            } else {
                // Unmapped type
                spdlog::warn("    UNMAPPED Type! {} ==> {}", bag->friendlyName, bag->value);
                std::cout << "\n";
            }
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
                spdlog::info("   Creation:    {}", tracker->creationTime.has_value() ? FormatTime(tracker->creationTime.value()) : "");
                std::cout << "\n";
                spdlog::info("   Volume Droid:       {}", tracker->volumeDroid);
                spdlog::info("   Volume Droid Birth: {}", tracker->volumeDroidBirth);
                spdlog::info("   File Droid:         {}", tracker->fileDroid);
                spdlog::info("   File Droid birth:   {}", tracker->fileDroidBirth);
                std::cout << "\n";
            } else if (auto console = std::dynamic_pointer_cast<ConsoleDataBlock>(eb)) {
                spdlog::info(">> Console data block");
                spdlog::info("   Fill Attributes: {}", console->fillAttributes);
                spdlog::info("   Popup Attributes: {}", console->popupFillAttributes);
                spdlog::info("   Buffer Size (Width x Height): {} x {}", console->screenWidthBufferSize, console->screenHeightBufferSize);
                spdlog::info("   Window Size (Width x Height): {} x {}", console->windowWidth, console->windowHeight);
                spdlog::info("   Origin (X/Y): {}/{}", console->windowOriginX, console->windowOriginY);
                spdlog::info("   Font Size: {}", console->fontSize);
                spdlog::info("   Is Bold: {}", console->fontWeight >= 700 ? "True" : "False");
                spdlog::info("   Face Name: {}", console->faceName);
                spdlog::info("   Cursor Size: {}", console->cursorSize);
                spdlog::info("   Is Full Screen: {}", console->fullScreen != 0 ? "True" : "False");
                spdlog::info("   Is Quick Edit: {}", console->quickEdit != 0 ? "True" : "False");
                spdlog::info("   Is Insert Mode: {}", console->insertMode != 0 ? "True" : "False");
                spdlog::info("   Is Auto Positioned: {}", console->autoPosition != 0 ? "True" : "False");
                spdlog::info("   History Buffer Size: {}", console->historyBufferSize);
                spdlog::info("   History Buffer Count: {}", console->numberOfHistoryBuffers);
                spdlog::info("   History Duplicates Allowed: {}", console->historyNoDup != 0 ? "True" : "False");
                std::cout << "\n";
            } else if (auto cfe = std::dynamic_pointer_cast<ConsoleFEDataBlock>(eb)) {
                spdlog::info(">> Console FE data block");
                spdlog::info("   Code page: {}", cfe->codePage);
                std::cout << "\n";
            } else if (auto darwin = std::dynamic_pointer_cast<DarwinDataBlock>(eb)) {
                spdlog::info(">> Darwin data block");
                spdlog::info("   Application ID: {}", darwin->applicationIdentifierUnicode);
                spdlog::info("   Product code: {}", darwin->productCode);
                spdlog::info("   Feature name: {}", darwin->featureName);
                spdlog::info("   Component ID: {}", darwin->componentId);
                std::cout << "\n";
            } else if (auto env = std::dynamic_pointer_cast<EnvironmentVariableDataBlock>(eb)) {
                spdlog::info(">> Environment variable data block");
                spdlog::info("   Environment variables: {}", env->environmentVariablesUnicode);
                std::cout << "\n";
            } else if (auto iconEnv = std::dynamic_pointer_cast<IconEnvironmentDataBlock>(eb)) {
                spdlog::info(">> Icon environment data block");
                spdlog::info("   Icon path: {}", iconEnv->iconPathUni);
                std::cout << "\n";
            } else if (auto kf = std::dynamic_pointer_cast<KnownFolderDataBlock>(eb)) {
                spdlog::info(">> Known folder data block");
                if (!kf->knownFolderName.empty()) {
                    spdlog::info("   Known folder GUID: {} ==> {}", kf->knownFolderId, kf->knownFolderName);
                } else {
                    spdlog::info("   Known folder GUID: {}", kf->knownFolderId);
                }
                std::cout << "\n";
            } else if (auto ps = std::dynamic_pointer_cast<PropertyStoreDataBlock>(eb)) {
                if (ps->sheets.empty()) {
                    spdlog::info(">> Property store data block");
                    spdlog::info("   (Property store is empty)");
                } else {
                    spdlog::info(">> Property store data block (Format: GUID\\ID Description ==> Value)");
                    int propCount = 0;
                    for (const auto& sheet : ps->sheets) {
                        for (const auto& prop : sheet.properties) {
                            propCount++;
                            const auto& key = std::get<0>(prop);
                            const auto& desc = std::get<1>(prop);
                            const auto& value = std::get<2>(prop);
                            std::string prefix = sheet.guid + "\\" + key;
                            prefix = prefix + std::string(43 - prefix.length(), ' ');
                            if (prefix.length() > 43) prefix = prefix.substr(0, 43);
                            std::string suffix = desc;
                            if (!suffix.empty()) {
                                suffix = suffix + std::string(35 - suffix.length(), ' ');
                                if (suffix.length() > 35) suffix = suffix.substr(0, 35);
                            }
                            spdlog::info("   {} {} ==> {}", prefix, suffix, value);
                        }
                    }
                    if (propCount == 0) {
                        spdlog::info("   (Property store is empty)");
                    }
                }
                std::cout << "\n";
            } else if (auto shim = std::dynamic_pointer_cast<ShimDataBlock>(eb)) {
                spdlog::info(">> Shimcache data block");
                spdlog::info("   Layer name: {}", shim->layerName);
                std::cout << "\n";
            } else if (auto sf = std::dynamic_pointer_cast<SpecialFolderDataBlock>(eb)) {
                spdlog::info(">> Special folder data block");
                spdlog::info("   Special Folder ID: {}", sf->specialFolderId);
                std::cout << "\n";
            } else if (auto vista = std::dynamic_pointer_cast<VistaAndAboveIdListDataBlock>(eb)) {
                spdlog::info(">> Vista and above ID List data block");
                for (const auto& bag : vista->targetIDs) {
                    std::string val = bag->value.empty() ? "(None)" : bag->value;
                    spdlog::info("   {} ==> {}", bag->friendlyName, val);
                }
                std::cout << "\n";
            } else if (auto dmg = std::dynamic_pointer_cast<DamagedDataBlock>(eb)) {
                spdlog::info(">> Damaged data block");
                spdlog::info("   Original Signature: {}", dmg->originalSignature);
                spdlog::info("   Error Message: {}", dmg->errorMessage);
                std::cout << "\n";
            } else {
                spdlog::info(">> {}", eb->GetTypeName());
                std::cout << "\n";
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
        spdlog::set_pattern("[%H:%M:%S.%e %l] %v%n");
    } else if (trace) {
        spdlog::set_level(spdlog::level::trace);
        spdlog::set_pattern("[%H:%M:%S.%e %l] %v%n");
    } else {
        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("%v%n");
    }

    g_dateFormat = dt;
    if (mp) {
        g_useMicroseconds = true;
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
        if (!quiet) {
            spdlog::info("Processing {}", filePath);
            std::cout << "\n";
        }
        auto start = std::chrono::steady_clock::now();
        auto lnk = LnkFile::Load(filePath, cp);
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();

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

        if (!quiet) {
            std::cout << "\n";
            spdlog::info("---------- Processed {} in {:.8f} seconds ----------", filePath, elapsed);
            std::cout << "\n";
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

        auto dirStart = std::chrono::steady_clock::now();
        for (const auto& f : lnkFiles) {
            if (!quiet) {
                spdlog::info("Processing {}", f);
                std::cout << "\n";
            }
            auto start = std::chrono::steady_clock::now();
            auto lnk = LnkFile::Load(f, cp);
            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();

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

            if (!quiet) {
                std::cout << "\n";
                spdlog::info("---------- Processed {} in {:.8f} seconds ----------", f, elapsed);
                std::cout << "\n";
            }
        }

        auto dirEnd = std::chrono::steady_clock::now();
        auto dirElapsed = std::chrono::duration_cast<std::chrono::duration<double>>(dirEnd - dirStart).count();
        if (!quiet) {
            std::cout << "\n";
        }
        spdlog::info("Processed {} out of {} files in {:.4f} seconds", lnkFiles.size() - failedFiles.size(), lnkFiles.size(), dirElapsed);
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
        csvEntries.push_back(GetCsvFormat(lnk, g_dateFormat, &macLookup, g_useMicroseconds));
    }

    // CSV output
    if (!csvDir.empty()) {
        if (!std::filesystem::exists(csvDir)) {
            std::filesystem::create_directories(csvDir);
        }
        std::string outName = tsStr + "_LECmd_Output.csv";
        if (!csvf.empty()) {
            outName = std::filesystem::path(csvf).filename().string();
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
        spdlog::info("Saving XML output to {}", xmlDir);
        XmlWriter xmlWriter;
        for (const auto& e : csvEntries) {
            std::string baseName = std::filesystem::path(e.sourceFile).filename().string();
            std::string outName = tsStr + "_" + baseName + ".xml";
            std::string outFile = (std::filesystem::path(xmlDir) / outName).string();
            if (!xmlWriter.WriteSingle(outFile, e)) {
                spdlog::error("Failed to write XML output for {}", e.sourceFile);
            }
        }
    }

    // HTML output
    if (!htmlDir.empty()) {
        if (!std::filesystem::exists(htmlDir)) {
            std::filesystem::create_directories(htmlDir);
        }
        std::string sanitizedHtml = htmlDir;
        for (auto& c : sanitizedHtml) {
            if (c == ':' || c == '\\' || c == '/') c = '_';
        }
        std::string outDirName = tsStr + "_LECmd_Output_for_" + sanitizedHtml;
        std::string outDir = (std::filesystem::path(htmlDir) / outDirName).string();
        if (!std::filesystem::exists(outDir)) {
            std::filesystem::create_directories(outDir);
        }
        std::string outFile = (std::filesystem::path(outDir) / "index.xhtml").string();
        spdlog::info("Saving HTML output to {}", outFile);
        HtmlWriter htmlWriter;
        if (!htmlWriter.Write(outFile, csvEntries)) {
            spdlog::error("Failed to write HTML output");
        }
        // Write CSS files
        std::ofstream nf((std::filesystem::path(outDir) / "normalize.css").string());
        if (nf.is_open()) { nf << HtmlWriter::GetNormalizeCss(); nf.close(); }
        std::ofstream sf((std::filesystem::path(outDir) / "style.css").string());
        if (sf.is_open()) { sf << HtmlWriter::GetStyleCss(); sf.close(); }
    }

    std::cout << "\n";
    spdlog::info("Results saved.");

    return 0;
}
