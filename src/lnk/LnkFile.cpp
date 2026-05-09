#include "LnkFile.h"
#include "PropertyStore.h"
#include <fstream>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include "utils/StringUtils.h"

namespace lecmd {

static uint16_t ReadUInt16LE(const uint8_t* data, size_t offset) {
    return data[offset] | (data[offset + 1] << 8);
}

static uint32_t ReadUInt32LE(const uint8_t* data, size_t offset) {
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

static uint64_t ReadUInt64LE(const uint8_t* data, size_t offset) {
    return static_cast<uint64_t>(ReadUInt32LE(data, offset)) |
           (static_cast<uint64_t>(ReadUInt32LE(data, offset + 4)) << 32);
}

static std::string ReadCp1252String(const uint8_t* data, size_t& offset, size_t maxLen = 1024) {
    size_t len = 0;
    for (size_t i = 0; i < maxLen && data[offset + i] != 0; ++i) {
        len++;
    }
    std::string result = DecodeCp1252(data + offset, len);
    offset += len;
    if (data[offset] == 0) ++offset;
    return result;
}

static std::string ReadAsciiString(const uint8_t* data, size_t& offset, size_t maxLen = 1024) {
    return ReadCp1252String(data, offset, maxLen);
}

static std::string ReadAsciiStringCount(const uint8_t* data, size_t& offset, uint16_t count) {
    std::string result = DecodeCp1252(data + offset, count);
    offset += count;
    return result;
}

static void AppendUtf8(std::string& out, uint32_t codepoint) {
    if (codepoint < 0x80) {
        out += static_cast<char>(codepoint);
    } else if (codepoint < 0x800) {
        out += static_cast<char>(0xC0 | (codepoint >> 6));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        out += static_cast<char>(0xE0 | (codepoint >> 12));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (codepoint >> 18));
        out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
}

static std::string ReadUnicodeString(const uint8_t* data, size_t& offset, size_t count) {
    std::string result;
    for (size_t i = 0; i < count; ++i) {
        uint16_t ch = ReadUInt16LE(data, offset + i * 2);
        if (ch == 0) {
            result += '\0';
            continue;
        }
        if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < count) {
            uint16_t low = ReadUInt16LE(data, offset + (i + 1) * 2);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                uint32_t codepoint = 0x10000 + ((ch - 0xD800) << 10) + (low - 0xDC00);
                AppendUtf8(result, codepoint);
                i++;
                continue;
            }
        }
        AppendUtf8(result, ch);
    }
    offset += count * 2;
    return result;
}

static std::string ReadUnicodeNullTerminated(const uint8_t* data, size_t& offset, size_t maxChars = 512) {
    std::string result;
    for (size_t i = 0; i < maxChars; ++i) {
        uint16_t ch = ReadUInt16LE(data, offset);
        offset += 2;
        if (ch == 0) break;
        if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < maxChars) {
            uint16_t low = ReadUInt16LE(data, offset);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                uint32_t codepoint = 0x10000 + ((ch - 0xD800) << 10) + (low - 0xDC00);
                AppendUtf8(result, codepoint);
                offset += 2;
                i++;
                continue;
            }
        }
        AppendUtf8(result, ch);
    }
    return result;
}

static std::string ReadUnicodeStringFromBytes(const uint8_t* data, size_t offset, uint32_t byteLen) {
    std::string result;
    uint32_t maxChars = byteLen / 2;
    for (uint32_t i = 0; i < maxChars; ++i) {
        uint16_t ch = ReadUInt16LE(data, offset + i * 2);
        if (ch == 0) {
            result += '\0';
            continue;
        }
        if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < maxChars) {
            uint16_t low = ReadUInt16LE(data, offset + (i + 1) * 2);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                uint32_t codepoint = 0x10000 + ((ch - 0xD800) << 10) + (low - 0xDC00);
                AppendUtf8(result, codepoint);
                i++;
                continue;
            }
        }
        AppendUtf8(result, ch);
    }
    return result;
}

static std::vector<std::string> SplitString(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    for (char c : s) {
        if (c == delimiter) {
            tokens.push_back(token);
            token.clear();
        } else {
            token += c;
        }
    }
    tokens.push_back(token);
    return tokens;
}

static int DarwinQuadToHex(const std::string& quad) {
    static const std::string base85 = "!$%&'()*+,-.0123456789=?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{}~";
    int dd = 0;
    for (int i = 4; i >= 0; --i) {
        size_t pos = base85.find(quad[i]);
        if (pos == std::string::npos) return 0;
        dd += static_cast<int>(pos);
        if (i > 0) dd *= 85;
    }
    return dd;
}

static std::string FlipHexPairs(const std::string& start) {
    std::vector<std::string> subs;
    for (size_t i = 0; i < 4; ++i) {
        subs.push_back(start.substr(i * 2, 2));
    }
    std::reverse(subs.begin(), subs.end());
    std::string result;
    for (const auto& s : subs) result += s;
    return result;
}

static std::string DecodeDarwinToGuid(const std::string& rawDarwin) {
    if (rawDarwin.length() < 20) return "";
    int hexChunks[4];
    for (int i = 0; i < 4; ++i) {
        hexChunks[i] = DarwinQuadToHex(rawDarwin.substr(i * 5, 5));
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%08X", hexChunks[0]);
    std::string b1 = buf;
    snprintf(buf, sizeof(buf), "%08X", hexChunks[1]);
    std::string b2 = buf;
    snprintf(buf, sizeof(buf), "%08X", hexChunks[2]);
    std::string b3 = buf;
    snprintf(buf, sizeof(buf), "%08X", hexChunks[3]);
    std::string b4 = buf;

    std::string block2left = b2.substr(0, 4);
    std::string block2right = b2.substr(4);
    std::string b3flipped = FlipHexPairs(b3);
    std::string block3left = b3flipped.substr(0, 4);
    std::string block3right = b3flipped.substr(4);
    std::string b4flipped = FlipHexPairs(b4);

    return "{" + b1 + "-" + block2right + "-" + block2left + "-" + block3left + "-" + block3right + b4flipped + "}";
}

static std::time_t FileTimeToTimeT(uint64_t filetime);
static std::time_t DosDateTimeToTimeT(uint32_t dosDateTime);

static std::time_t FileTimeToTimeT(uint64_t filetime) {
    if (filetime == 0) return 0;
    // FILETIME is 100-nanosecond intervals since January 1, 1601
    // Unix time is seconds since January 1, 1970
    const uint64_t EPOCH_DIFF = 116444736000000000ULL;
    if (filetime < EPOCH_DIFF) return 0;
    return static_cast<std::time_t>((filetime - EPOCH_DIFF) / 10000000);
}

static std::string GuidToString(const uint8_t* data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << ReadUInt32LE(data, 0) << "-";
    oss << std::setw(4) << ReadUInt16LE(data, 4) << "-";
    oss << std::setw(4) << ReadUInt16LE(data, 6) << "-";
    oss << std::setw(2) << (int)data[8] << std::setw(2) << (int)data[9] << "-";
    for (int i = 10; i < 16; ++i) oss << std::setw(2) << (int)data[i];
    return oss.str();
}

static std::string BytesToHex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << (int)data[i];
    }
    return oss.str();
}

static std::string MacToString(const uint8_t* data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 6; ++i) {
        if (i > 0) oss << ":";
        oss << std::setw(2) << (int)data[i];
    }
    return oss.str();
}

std::shared_ptr<LnkFile> LnkFile::Load(const std::string& path, int codepage) {
    auto lnk = std::make_shared<LnkFile>();
    lnk->sourceFile = path;

    std::ifstream file(path, std::ios::binary);
    if (!file) return nullptr;

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size < 76) return nullptr;

    std::vector<uint8_t> raw(size);
    file.read(reinterpret_cast<char*>(raw.data()), size);

    if (!lnk->Parse(raw, codepage)) return nullptr;

    // Get source file timestamps
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        lnk->sourceCreated = st.st_ctime;
        lnk->sourceModified = st.st_mtime;
        lnk->sourceAccessed = st.st_atime;
    }

    return lnk;
}

bool LnkFile::Parse(const std::vector<uint8_t>& raw, int codepage) {
    codepage_ = codepage;
    if (raw.size() < 76) return false;

    const uint8_t* data = raw.data();

    // Parse header
    header.headerSize = ReadUInt32LE(data, 0);
    if (header.headerSize != 0x4C) return false;

    std::memcpy(header.clsid, data + 4, 16);
    // Check CLSID {00021401-0000-0000-C000-000000000046}
    static const uint8_t expectedClsid[16] = {
        0x01, 0x14, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46
    };
    if (std::memcmp(header.clsid, expectedClsid, 16) != 0) return false;

    header.dataFlags = ReadUInt32LE(data, 20);
    header.fileAttributes = ReadUInt32LE(data, 24);
    header.targetCreationTime = ReadUInt64LE(data, 28);
    header.targetAccessTime = ReadUInt64LE(data, 36);
    header.targetModificationTime = ReadUInt64LE(data, 44);
    header.fileSize = ReadUInt32LE(data, 52);
    header.iconIndex = static_cast<int32_t>(ReadUInt32LE(data, 56));
    header.showWindow = ReadUInt32LE(data, 60);
    header.hotKey = ReadUInt16LE(data, 64);
    header.reserved0 = ReadUInt16LE(data, 66);
    header.reserved1 = ReadUInt32LE(data, 68);
    header.reserved2 = ReadUInt32LE(data, 72);

    size_t index = 76;

    // Link Target ID List
    if (header.HasFlag(LnkHeader::HasTargetIdList)) {
        ParseTargetIDList(raw, index);
    }

    // Link Info
    if (header.HasFlag(LnkHeader::HasLinkInfo)) {
        ParseLinkInfo(raw, index);
    }

    // String Data
    ParseStringData(raw, index);

    // Extra Data
    if (index < raw.size()) {
        ParseExtraData(raw, index);
    }

    return true;
}

void LnkFile::ParseTargetIDList(const std::vector<uint8_t>& raw, size_t& index) {
    if (index + 2 > raw.size()) return;
    uint16_t listSize = ReadUInt16LE(raw.data(), index);
    index += 2;

    size_t listEnd = index + listSize;
    if (listEnd > raw.size()) return;

    while (index < listEnd) {
        if (index + 2 > listEnd) break;
        uint16_t itemSize = ReadUInt16LE(raw.data(), index);
        if (itemSize == 0) break;
        if (index + itemSize > listEnd) break;

        std::vector<uint8_t> itemData(raw.begin() + index, raw.begin() + index + itemSize);
        auto bag = ParseShellItem(itemData, codepage_);
        if (bag) {
            targetIDs.push_back(bag);
        }
        index += itemSize;
    }
    // Ensure index is at listEnd (skips terminal ID 0x0000 and any padding)
    index = listEnd;
}

static std::string GetKnownFolderName(const std::string& guid);

static std::string ExtractPropertyViewValue(const std::vector<uint8_t>& propBytes) {
    PropertyStore store(propBytes);
    for (const auto& sheet : store.sheets) {
        for (const auto& prop : sheet.properties) {
            if (std::get<0>(prop) == "10") {
                return std::get<2>(prop);
            }
        }
    }
    std::string result;
    for (const auto& sheet : store.sheets) {
        for (const auto& prop : sheet.properties) {
            if (!result.empty()) result += "::";
            result += std::get<2>(prop);
        }
    }
    return result.empty() ? "No Property sheet value found" : result;
}

std::shared_ptr<ShellBag> LnkFile::ParseShellItem(const std::vector<uint8_t>& raw, int codepage) {
    if (raw.size() < 3) return nullptr;

    auto bag = std::make_shared<ShellBag>();
    bag->size = raw[0] | (raw[1] << 8);
    bag->type = raw[2];

    const uint8_t* data = raw.data();
    size_t len = raw.size();

    // Check for zip contents
    if (len >= 0x28) {
        uint64_t sig1 = ReadUInt64LE(data, 0x8);
        uint64_t sig2 = ReadUInt64LE(data, 0x18);
        if (sig1 == 0 && sig2 == 0) {
            if (data[0x28] == 0x2f || data[0x26] == 0x2f || data[0x1a] == 0x2f || data[0x1c] == 0x2f) {
                bag->friendlyName = "Zip Contents";
                bag->value = "ZIP";
                return bag;
            }
        }
    }

    switch (bag->type) {
        case 0x1f: {
            // GUID only (0x14 size)
            if (raw.size() == 0x14) {
                bag->friendlyName = "Root folder: GUID";
                bag->value = GuidToString(data + 4);
                break;
            }
            // Drive letter
            if (len >= 7 && data[4] == 0x2f) {
                bag->friendlyName = "Users property view: Drive letter";
                bag->value = std::string(1, static_cast<char>(data[13])) + ":";
                break;
            }
            // off3 bitmask handling
            if (len >= 4) {
                uint8_t off3 = data[3];
                uint8_t off3Bitmask = off3 & 0x70;
                if (off3Bitmask == 0x40 || off3Bitmask == 0x50 || off3Bitmask == 0x70) {
                    bag->friendlyName = "Root folder: GUID";
                    bag->value = GuidToString(data + 4);
                    size_t idx = 20;
                    if (idx + 8 <= len) {
                        ParseExtensionBlocks(bag.get(), raw, idx, len - idx, codepage);
                    }
                    break;
                }
            }
            // Windows Backup
            if (len >= 10) {
                uint32_t dataSig = ReadUInt32LE(data, 6);
                if (dataSig == 0x4c644970) {
                    bag->friendlyName = "Windows Backup";
                    if (len >= 0x0c + 8) {
                        bag->backupDateTime = FileTimeToTimeT(ReadUInt64LE(data, 0x0c));
                    }
                    if (len >= 0x14 + 8) {
                        bag->modifiedDateFromBackup = FileTimeToTimeT(ReadUInt64LE(data, 0x14));
                    }
                    if (len >= 0x1c + 8) {
                        bag->createdDateFromBackup = FileTimeToTimeT(ReadUInt64LE(data, 0x1c));
                    }
                    if (len >= 0x24 + 8) {
                        bag->backupUnknownDateTime = FileTimeToTimeT(ReadUInt64LE(data, 0x24));
                    }
                    size_t bidx = 0x2c + 12; // after 4 FILETIMEs + 12 unknown
                    if (bidx + 2 <= len) {
                        uint16_t nameLen = ReadUInt16LE(data, bidx);
                        bidx += 2;
                        if (bidx + nameLen * 2 <= len) {
                            bag->value = ReadUnicodeStringFromBytes(data, bidx, nameLen * 2);
                            bidx += nameLen * 2;
                        }
                    }
                    bidx += 4; // unknown
                    if (bidx + 32 <= len) {
                        bidx += 16; // skip first GUID
                        bag->classId = GetKnownFolderName(GuidToString(data + bidx));
                    }
                    break;
                }
            }
            // Property view default
            if (len >= 10) {
                uint32_t dataSig = ReadUInt32LE(data, 6);
                if (dataSig == 0xbeebee00) {
                    bag->friendlyName = "Variable: Users property view";
                    uint16_t shellPropertySheetListSize = ReadUInt16LE(data, 10);
                    uint16_t identifierSize = ReadUInt16LE(data, 12);
                    size_t idx = 14;
                    if (identifierSize > 0 && idx + identifierSize <= len) {
                        idx += identifierSize;
                    }
                    if (shellPropertySheetListSize > 0 && idx + shellPropertySheetListSize <= len) {
                        std::vector<uint8_t> propBytes(data + idx, data + idx + shellPropertySheetListSize);
                        PropertyStore ps(propBytes);
                        // Populate bag->propertyStore
                        for (const auto& sheet : ps.sheets) {
                            for (const auto& prop : sheet.properties) {
                                bag->propertyStore.emplace_back(std::get<0>(prop), std::get<2>(prop));
                            }
                        }
                        bag->value = ExtractPropertyViewValue(propBytes);
                        // BEEF scan inside propBytes if key "32" exists
                        bool has32 = false;
                        for (const auto& sheet : ps.sheets) {
                            for (const auto& prop : sheet.properties) {
                                if (std::get<0>(prop) == "32") { has32 = true; break; }
                            }
                        }
                        if (has32) {
                            for (size_t i = 0; i + 7 < propBytes.size(); ++i) {
                                if (propBytes[i+4] == 0xEF && propBytes[i+5] == 0xBE) {
                                    uint16_t exSize = ReadUInt16LE(propBytes.data(), i);
                                    if (exSize > 0 && i + exSize <= propBytes.size()) {
                                        std::vector<uint8_t> exBlock(propBytes.begin() + i, propBytes.begin() + i + exSize);
                                        size_t exOff = 0;
                                        auto block = ParseExtensionBlock(exBlock, exOff, codepage);
                                        if (block) bag->extensionBlocks.push_back(block);
                                    }
                                }
                            }
                        }
                    } else {
                        bag->value = "!!! Unable to determine Value !!!";
                    }
                    idx += shellPropertySheetListSize + 2;
                    if (idx + 8 <= len) {
                        ParseExtensionBlocks(bag.get(), raw, idx, len - idx, codepage);
                    }
                    break;
                }
            }
            // GUID + single extension block (size 50 or 58)
            if (raw.size() == 50 || raw.size() == 58) {
                bag->friendlyName = "Root folder: GUID";
                bag->value = GuidToString(data + 4);
                size_t idx = 20;
                if (idx + 8 <= len) {
                    ParseExtensionBlocks(bag.get(), raw, idx, len - idx, codepage);
                }
                break;
            }
            // Default
            bag->friendlyName = "Drive / Computer";
            if (len >= 20) {
                bag->value = GuidToString(data + 4);
            }
            break;
        }
        case 0x22:
        case 0x23: {
            bag->friendlyName = "Drive letter";
            if (len >= 5) {
                bag->value = DecodeCp1252(data + 3, 2);
                size_t np = bag->value.find('\0');
                if (np != std::string::npos) bag->value = bag->value.substr(0, np);
            }
            // Extension blocks at offset 0x19 if len > 0x30
            if (len > 0x30) {
                size_t extOff = 0x19;
                if (extOff + 8 <= len) {
                    std::vector<uint8_t> exBlock(data + extOff, data + len);
                    size_t exOff = 0;
                    auto block = ParseExtensionBlock(exBlock, exOff, codepage);
                    if (block) bag->extensionBlocks.push_back(block);
                }
            }
            break;
        }
        case 0x2a:
        case 0x2f: {
            bag->friendlyName = "Drive letter";
            if (len >= 5) {
                bag->value = DecodeCp1252(data + 3, 2);
                size_t np = bag->value.find('\0');
                if (np != std::string::npos) bag->value = bag->value.substr(0, np);
            }
            // Extension blocks at offset 0x19 if len > 0x30
            if (len > 0x30) {
                size_t extOff = 0x19;
                if (extOff + 8 <= len) {
                    std::vector<uint8_t> exBlock(data + extOff, data + len);
                    size_t exOff = 0;
                    auto block = ParseExtensionBlock(exBlock, exOff, codepage);
                    if (block) bag->extensionBlocks.push_back(block);
                }
            }
            break;
        }
        case 0x2e: {
            if (len >= 4 && (data[3] == 0x80 || len == 0x16)) {
                // GUID only
                bag->friendlyName = "Root folder: GUID";
                if (len >= 20) {
                    bag->value = GuidToString(data + 4);
                }
                size_t idx = 20;
                if (idx + 8 <= len) {
                    ParseExtensionBlocks(bag.get(), raw, idx, len - idx, codepage);
                }
            } else if (len >= 8) {
                uint64_t postSig = ReadUInt64LE(data, len - 8);
                if (postSig == 0x0000ee306bfe9555ULL || postSig == 0xee306bfe9555c589ULL) {
                    // User profile
                    bag->friendlyName = "User profile";
                    if (len >= 14) {
                        bag->lastAccessTime = DosDateTimeToTimeT(ReadUInt32LE(data, len - 14));
                    }
                    size_t idx = 10;
                    if (idx < len) {
                        std::string tempStr = ReadUnicodeStringFromBytes(data, idx, static_cast<uint32_t>((len - idx) * 2));
                        size_t nullPos = tempStr.find('\0');
                        if (nullPos != std::string::npos) tempStr = tempStr.substr(0, nullPos);
                        if (tempStr.empty()) tempStr = "(None)";
                        bag->value = tempStr;
                    }
                } else {
                    int32_t testSig2 = static_cast<int32_t>(ReadUInt32LE(data, 5));
                    if (testSig2 >= 0x15032601) {
                        // Control panel category
                        bag->friendlyName = "Control panel category";
                        if (len < 0x48) {
                            if (len >= 20) {
                                bag->devicePath = GetKnownFolderName(GuidToString(data + 4));
                                bag->category = bag->devicePath;
                            }
                            size_t idx = 20;
                            if (idx + 8 <= len) {
                                std::vector<uint8_t> b26Bytes(data + idx, data + len);
                                size_t b26Off = 0;
                                auto block = ParseExtensionBlock(b26Bytes, b26Off, codepage);
                                if (block) {
                                    bag->extensionBlocks.push_back(block);
                                }
                            }
                        } else {
                            size_t idx = 0x12;
                            if (idx + 0x48 * 2 <= len) {
                                bag->devicePath = ReadUnicodeStringFromBytes(data, idx, 0x48 * 2);
                            }
                            idx = 0x116;
                            if (idx < len && len >= 0x22) {
                                size_t val2Len = len - 0x22 - idx;
                                if (val2Len > 0 && val2Len <= len) {
                                    bag->value = ReadUnicodeStringFromBytes(data, idx, static_cast<uint32_t>(val2Len * 2));
                                }
                            }
                            if (len >= 0x22 + 32) {
                                size_t guidOff = len - 0x22 + 16; // second GUID
                                bag->category = GetKnownFolderName(GuidToString(data + guidOff));
                            }
                        }
                    } else {
                        // Property view default (same as 0x1f property view)
                        bag->friendlyName = "Users property view";
                        if (len >= 12) {
                            uint16_t shellPropertySheetListSize = ReadUInt16LE(data, 10);
                            uint16_t identifierSize = ReadUInt16LE(data, 12);
                            size_t idx = 14;
                            if (identifierSize > 0 && idx + identifierSize <= len) {
                                idx += identifierSize;
                            }
                            if (shellPropertySheetListSize > 0 && idx + shellPropertySheetListSize <= len) {
                                std::vector<uint8_t> propBytes(data + idx, data + idx + shellPropertySheetListSize);
                                bag->value = ExtractPropertyViewValue(propBytes);
                                try {
                                    PropertyStore tmpStore(propBytes);
                                    bool has32 = false;
                                    for (const auto& sheet : tmpStore.sheets) {
                                        for (const auto& prop : sheet.properties) {
                                            if (std::get<0>(prop) == "32") { has32 = true; break; }
                                        }
                                    }
                                    if (has32) {
                                        for (size_t i = 0; i + 7 < propBytes.size(); ++i) {
                                            if (propBytes[i+4] == 0xEF && propBytes[i+5] == 0xBE) {
                                                uint16_t exSize = ReadUInt16LE(propBytes.data(), i);
                                                if (exSize > 0 && i + exSize <= propBytes.size()) {
                                                    std::vector<uint8_t> exBlock(propBytes.begin() + i, propBytes.begin() + i + exSize);
                                                    size_t exOff = 0;
                                                    auto block = ParseExtensionBlock(exBlock, exOff, codepage);
                                                    if (block) bag->extensionBlocks.push_back(block);
                                                }
                                            }
                                        }
                                    }
                                } catch (...) {}
                            }
                            idx += shellPropertySheetListSize + 2;
                            if (idx + 16 <= len) {
                                idx += 16; // skip first GUID
                            }
                            if (idx + 16 <= len) {
                                std::string guid2 = GuidToString(data + idx);
                                std::string name = GetKnownFolderName(guid2);
                                if (!name.empty()) bag->value = name;
                                idx += 16;
                            }
                            // Extension blocks
                            if (idx + 8 <= len) {
                                int16_t extBlockSize = static_cast<int16_t>(ReadUInt16LE(data, idx));
                                while (extBlockSize > 0 && idx + extBlockSize <= len) {
                                    std::vector<uint8_t> exBlock(data + idx, data + idx + extBlockSize);
                                    size_t exOff = 0;
                                    auto block = ParseExtensionBlock(exBlock, exOff, codepage);
                                    if (block) bag->extensionBlocks.push_back(block);
                                    idx += extBlockSize;
                                    if (idx + 2 <= len) {
                                        extBlockSize = static_cast<int16_t>(ReadUInt16LE(data, idx));
                                    } else {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            break;
        }
        case 0xb1:
        case 0x31:
        case 0x3A:
        case 0x35:
        case 0x39: {
            bag->friendlyName = "Directory";
            // ZIP contents detection
            if (len > 0x29 && (
                (data[0x27] == 0x00 && data[0x28] == 0x2f && data[0x29] == 0x00) ||
                (data[0x24] == 0x4e && data[0x26] == 0x2f && data[0x28] == 0x41))) {
                if (data[0x28] == 0x2f || data[0x26] == 0x2f || data[0x1a] == 0x2f || data[0x1c] == 0x2f) {
                    bag->friendlyName = "Zip file contents";
                    // Parse ZIP contents (simplified ShellBagZipContents)
                    size_t zipIdx = 84;
                    if (len > 0x14 && data[0x14] == 0x10) zipIdx = 60;
                    try {
                        uint16_t nameSize1 = ReadUInt16LE(data, zipIdx);
                        zipIdx += 4;
                        uint16_t nameSize2 = ReadUInt16LE(data, zipIdx);
                        zipIdx += 4;
                        if (nameSize1 > 0 && zipIdx + nameSize1 * 2 <= len) {
                            bag->value = ReadUnicodeStringFromBytes(data, zipIdx, nameSize1 * 2);
                            zipIdx += nameSize1 * 2 + 2;
                        }
                        if (nameSize2 > 0 && zipIdx + nameSize2 * 2 <= len) {
                            zipIdx += nameSize2 * 2 + 2;
                        }
                        // Extract date
                        std::string rawdatestr;
                        if (len >= 0x24 + 40) {
                            rawdatestr = ReadUnicodeStringFromBytes(data, 0x24, 40);
                            size_t np = rawdatestr.find('\0');
                            if (np != std::string::npos) rawdatestr = rawdatestr.substr(0, np);
                        }
                        if (!rawdatestr.empty()) {
                            // Try parse date
                            struct tm t = {};
                            if (sscanf(rawdatestr.c_str(), "%d/%d/%d", &t.tm_mon, &t.tm_mday, &t.tm_year) == 3) {
                                t.tm_year -= 1900;
                                t.tm_mon -= 1;
                                bag->lastAccessTime = timegm(&t);
                            }
                        }
                        if (!bag->value.empty() && bag->value.find("Unable to determine value") == std::string::npos) {
                            break;
                        }
                    } catch (...) {}
                }
            }
            if (len >= 14) {
                size_t off = 3; // skip type
                off += 1; // skip unknown
                off += 4; // skip file size (always 0 for directory)
                // Last modification time (DOS date/time)
                if (off + 4 <= len) {
                    bag->lastModificationTime = DosDateTimeToTimeT(ReadUInt32LE(data, off));
                }
                off += 4;
                off += 2; // skip unknown

                // Find extension block position by searching for BEEF0004 pattern
                int beefPos = -1;
                for (size_t i = off; i + 7 < len; ++i) {
                    if (data[i] == 0x04 && data[i+1] == 0x00 && data[i+2] == 0xEF && data[i+3] == 0xBE) {
                        beefPos = static_cast<int>(i);
                        break;
                    }
                }

                size_t nameLen = 0;
                if (bag->type == 0x35 && beefPos > 0) {
                    // Unicode short name: use full distance to BEEF block (C# behavior)
                    nameLen = beefPos - off;
                } else {
                    while (off + nameLen < len && data[off + nameLen] != 0) {
                        nameLen++;
                    }
                    // Cap name length at beefPos if found
                    if (beefPos > 0 && static_cast<size_t>(beefPos) > off && nameLen > static_cast<size_t>(beefPos) - off) {
                        nameLen = beefPos - off;
                    }
                }

                if (nameLen > 0 && off + nameLen <= len) {
                    std::string shortName;
                    if (bag->type == 0x35) {
                        // Unicode short name for 0x35
                        for (size_t i = 0; i + 1 < nameLen; i += 2) {
                            uint16_t ch = data[off + i] | (data[off + i + 1] << 8);
                            if (ch == 0) break;
                            AppendUtf8(shortName, ch);
                        }
                    } else {
                        shortName = DecodeCp1252(data + off, nameLen);
                    }
                    bag->shortName = shortName;
                    bag->value = shortName;
                    off += nameLen;
                }

                // Skip null terminators and padding
                while (off < len && data[off] == 0) off++;

                // Parse extension blocks
                if (off + 8 <= len) {
                    ParseExtensionBlocks(bag.get(), raw, off, len - off, codepage);
                }
            }
            break;
        }
        case 0x32:
        case 0x36: {
            bag->friendlyName = "File";
            // ZIP contents detection
            if (len > 0x28 && (data[0x28] == 0x2f || (data[0x24] == 0x4e && data[0x26] == 0x2f && data[0x28] == 0x41))) {
                if (data[0x28] == 0x2f || data[0x26] == 0x2f || data[0x1a] == 0x2f || data[0x1c] == 0x2f) {
                    bag->friendlyName = "Zip file contents";
                    size_t zipIdx = 84;
                    if (len > 0x14 && data[0x14] == 0x10) zipIdx = 60;
                    try {
                        uint16_t nameSize1 = ReadUInt16LE(data, zipIdx);
                        zipIdx += 4;
                        uint16_t nameSize2 = ReadUInt16LE(data, zipIdx);
                        zipIdx += 4;
                        if (nameSize1 > 0 && zipIdx + nameSize1 * 2 <= len) {
                            bag->value = ReadUnicodeStringFromBytes(data, zipIdx, nameSize1 * 2);
                            zipIdx += nameSize1 * 2 + 2;
                        }
                        if (nameSize2 > 0 && zipIdx + nameSize2 * 2 <= len) {
                            zipIdx += nameSize2 * 2 + 2;
                        }
                        std::string rawdatestr;
                        if (len >= 0x24 + 40) {
                            rawdatestr = ReadUnicodeStringFromBytes(data, 0x24, 40);
                            size_t np = rawdatestr.find('\0');
                            if (np != std::string::npos) rawdatestr = rawdatestr.substr(0, np);
                        }
                        if (!rawdatestr.empty()) {
                            struct tm t = {};
                            if (sscanf(rawdatestr.c_str(), "%d/%d/%d", &t.tm_mon, &t.tm_mday, &t.tm_year) == 3) {
                                t.tm_year -= 1900;
                                t.tm_mon -= 1;
                                bag->lastAccessTime = timegm(&t);
                            }
                        }
                        if (!bag->value.empty() && bag->value.find("Unable to determine value") == std::string::npos) {
                            break;
                        }
                    } catch (...) {}
                }
            }
            if (len >= 14) {
                size_t off = 3; // skip type
                off += 1; // skip unknown
                // File size
                off += 4;
                // Last modification time
                if (off + 4 <= len) {
                    bag->lastModificationTime = DosDateTimeToTimeT(ReadUInt32LE(data, off));
                }
                off += 4;
                off += 2; // skip unknown

                int beefPos = -1;
                for (size_t i = off; i + 7 < len; ++i) {
                    if (data[i] == 0x04 && data[i+1] == 0x00 && data[i+2] == 0xEF && data[i+3] == 0xBE) {
                        beefPos = static_cast<int>(i);
                        break;
                    }
                }

                // No-BEEF fallback for 0x32
                if (beefPos == 0 && bag->type == 0x32) {
                    std::string hackName = DecodeCp1252(data + off, len - off);
                    std::vector<std::string> segs;
                    std::string cur;
                    for (char c : hackName) {
                        if (c == '\0') {
                            if (!cur.empty()) segs.push_back(cur);
                            cur.clear();
                        } else {
                            cur += c;
                        }
                    }
                    if (!cur.empty()) segs.push_back(cur);
                    std::string shortName;
                    for (size_t i = 0; i < segs.size(); ++i) {
                        if (i > 0) shortName += "|";
                        shortName += segs[i];
                    }
                    bag->shortName = shortName;
                    bag->value = shortName;
                    break;
                }

                size_t nameLen = 0;
                if (bag->type == 0x36 && beefPos > 0) {
                    nameLen = beefPos - off;
                } else {
                    while (off + nameLen < len && data[off + nameLen] != 0) {
                        nameLen++;
                    }
                    if (beefPos > 0 && static_cast<size_t>(beefPos) > off && nameLen > static_cast<size_t>(beefPos) - off) {
                        nameLen = beefPos - off;
                    }
                }

                if (nameLen > 0 && off + nameLen <= len) {
                    std::string shortName;
                    if (bag->type == 0x36) {
                        // Unicode short name for 0x36
                        for (size_t i = 0; i + 1 < nameLen; i += 2) {
                            uint16_t ch = data[off + i] | (data[off + i + 1] << 8);
                            if (ch == 0) break;
                            AppendUtf8(shortName, ch);
                        }
                    } else {
                        shortName = DecodeCp1252(data + off, nameLen);
                    }
                    bag->shortName = shortName;
                    bag->value = shortName;
                    off += nameLen;
                }

                while (off < len && data[off] == 0) off++;

                if (off + 8 <= len) {
                    ParseExtensionBlocks(bag.get(), raw, off, len - off, codepage);
                }
            }
            break;
        }
        case 0x00: {
            // Check for special signatures at offset 4
            if (len >= 8) {
                uint32_t specialSig = ReadUInt32LE(data, 4);
                if (specialSig == 0xc001b000) {
                    bag->friendlyName = "Variable: HTTP URI";
                    if (len >= 0x18) {
                        size_t off = 0x14;
                        uint32_t urlSize = ReadUInt32LE(data, off);
                        off += 4;
                        if (off + urlSize <= len) {
                            std::string url = ReadUnicodeStringFromBytes(data, off, urlSize);
                            // URI unescape (basic: replace %XX with char)
                            std::string unescaped;
                            for (size_t i = 0; i < url.size(); ++i) {
                                if (url[i] == '%' && i + 2 < url.size()) {
                                    int hex = std::stoi(url.substr(i + 1, 2), nullptr, 16);
                                    unescaped += static_cast<char>(hex);
                                    i += 2;
                                } else {
                                    unescaped += url[i];
                                }
                            }
                            bag->value = unescaped;
                        }
                        off += urlSize;
                        if (off + 16 <= len) off += 16;
                        if (off + 4 <= len) {
                            uint32_t urlSize2 = ReadUInt32LE(data, off);
                            off += 4;
                            if (off + urlSize2 <= len) {
                                bag->fullUrl = ReadUnicodeStringFromBytes(data, off, urlSize2);
                            }
                        }
                    }
                    break;
                }
                if (specialSig == 0x49534647) { // "GFSI"
                    bag->friendlyName = "Variable: Game folder";
                    break;
                }
                if (specialSig == 0xffffff38) {
                    bag->friendlyName = "Variable: Control panel CPL";
                    bag->value = "!!! Send this hive to saericzimmerman@gmail.com so support can be added !!!";
                    break;
                }
            }
            // Server name (short item)
            if (len <= 0x60 && len >= 6) {
                bag->friendlyName = "Server name";
                bag->value = ReadUnicodeStringFromBytes(data, 6, (len - 6) * 2);
                break;
            }
            // Check data signature at offset 6
            if (len >= 10) {
                uint16_t dataSize = ReadUInt16LE(data, 4);
                uint32_t dataSig = ReadUInt32LE(data, 6);

                // FTP sub item
                if (dataSig == 0x00030005 || dataSig == 0x00000005) {
                    bag->friendlyName = "Variable: FTP URI";
                    if (len >= 0x16 + 16) {
                        size_t off = 0x16;
                        uint64_t ft = ReadUInt64LE(data, off);
                        if (ft > 0) {
                            bag->ftpFolderTime = FileTimeToTimeT(ft);
                        }
                        off += 16; // skip two FILETIMEs
                        size_t len1 = 0;
                        while (off + len1 < len && data[off + len1] != 0) len1++;
                        bag->shortName = DecodeCp1252(data + off, len1);
                        off += len1;
                        while (off < len && data[off] == 0) off++;
                        len1 = 0;
                        while (off + len1 + 1 < len && !(data[off + len1] == 0 && data[off + len1 + 1] == 0)) len1++;
                        bag->value = ReadUnicodeStringFromBytes(data, off, len1 + 1);
                    }
                    break;
                }

                // Property view GUID
                if (dataSig == 0x23febbee) {
                    bag->friendlyName = "Variable: Users property view";
                    size_t off = 4;
                    off += 2; // dataSize
                    off += 4; // skip signature
                    uint16_t propertyStoreSize = ReadUInt16LE(data, off);
                    off += 2;
                    uint16_t identifierSize = ReadUInt16LE(data, off);
                    off += 2;
                    if (identifierSize > 0 && off + 16 <= len) {
                        bag->value = GuidToString(data + off);
                        off += 16;
                    }
                    off += 2; // skip 2 unknown
                    if (off + 8 <= len) {
                        ParseExtensionBlocks(bag.get(), raw, off, len - off, codepage);
                    }
                    break;
                }

                // MTP type 2
                if (dataSig == 0x10312005) {
                    bag->friendlyName = "Variable: MTP type 2";
                    size_t off = 4;
                    off += 2; // dataSize
                    off += 4; // skip signature
                    off += 4; // skip unknown
                    off += 2; // skip unknown
                    off += 2; // skip unknown
                    off += 4; // skip unknown
                    off += 12; // skip unknown empty
                    off += 4; // skip unknown size
                    if (off + 12 <= len) {
                        int32_t storageStringNameLen = static_cast<int32_t>(ReadUInt32LE(data, off));
                        off += 4;
                        int32_t storageIdStringLen = static_cast<int32_t>(ReadUInt32LE(data, off));
                        off += 4;
                        int32_t fileSystemNameLen = static_cast<int32_t>(ReadUInt32LE(data, off));
                        off += 4;
                        int32_t numGuids = static_cast<int32_t>(ReadUInt32LE(data, off));
                        off += 8; // skip unknown + numGuids
                        if (storageStringNameLen > 0 && off + storageStringNameLen * 2 <= len) {
                            bag->value = ReadUnicodeStringFromBytes(data, off, storageStringNameLen * 2 - 2);
                            off += storageStringNameLen * 2;
                        }
                        if (storageIdStringLen > 0 && off + storageIdStringLen * 2 <= len) {
                            bag->storageIdName = ReadUnicodeStringFromBytes(data, off, storageIdStringLen * 2 - 2);
                            off += storageIdStringLen * 2;
                        }
                        if (fileSystemNameLen > 0 && off + fileSystemNameLen * 2 <= len) {
                            bag->fileSystemName = ReadUnicodeStringFromBytes(data, off, fileSystemNameLen * 2 - 2);
                            off += fileSystemNameLen * 2;
                        }
                        for (int i = 0; i < numGuids && off + 78 <= len; ++i) {
                            bag->mtpGuids.push_back(ReadUnicodeStringFromBytes(data, off, 78));
                            off += 78;
                        }
                        off += 4; // unknown
                        if (off + 16 <= len) {
                            bag->classId = GetKnownFolderName(GuidToString(data + off));
                            off += 16;
                        }
                    }
                    break;
                }

                // MTP type 1
                if (dataSig == 0x07192006) {
                    bag->friendlyName = "Variable: MTP type 1";
                    size_t off = 0x1a;
                    if (off + 16 <= len) {
                        bag->lastModificationTime = FileTimeToTimeT(ReadUInt64LE(data, off));
                        off += 8;
                        bag->createdOnTime = FileTimeToTimeT(ReadUInt64LE(data, off));
                        off += 8;
                        bag->mtpType1GuidName = GetKnownFolderName(GuidToString(data + off));
                        off += 16;
                    }
                    off = 0x3e;
                    if (off + 12 <= len) {
                        int32_t storageStringNameLen = static_cast<int32_t>(ReadUInt32LE(data, off));
                        off += 4;
                        int32_t storageIdStringLen = static_cast<int32_t>(ReadUInt32LE(data, off));
                        off += 4;
                        int32_t fileSystemNameLen = static_cast<int32_t>(ReadUInt32LE(data, off));
                        off += 4;
                        std::string storageName;
                        if (storageStringNameLen > 0 && off + storageStringNameLen * 2 <= len) {
                            storageName = ReadUnicodeStringFromBytes(data, off, storageStringNameLen * 2 - 2);
                            off += storageStringNameLen * 2;
                        }
                        if (storageIdStringLen > 0 && off + storageIdStringLen * 2 <= len) {
                            bag->storageIdName = ReadUnicodeStringFromBytes(data, off, storageIdStringLen * 2 - 2);
                            off += storageIdStringLen * 2;
                        }
                        if (fileSystemNameLen > 0 && off + fileSystemNameLen * 2 <= len) {
                            bag->fileSystemName = ReadUnicodeStringFromBytes(data, off, fileSystemNameLen * 2 - 2);
                            off += fileSystemNameLen * 2;
                        }
                        if (!storageName.empty()) {
                            if (storageName == bag->storageIdName) {
                                bag->value = storageName;
                            } else {
                                bag->value = storageName + " (" + bag->storageIdName + ")";
                            }
                        } else {
                            bag->value = bag->storageIdName;
                        }
                    }
                    break;
                }

                // Property view default (0xbeebee00)
                if (dataSig == 0xbeebee00) {
                    bag->friendlyName = "Variable: Users property view";
                    uint16_t shellPropertySheetListSize = ReadUInt16LE(data, 10);
                    uint16_t identifierSize = ReadUInt16LE(data, 12);
                    if (identifierSize > len) {
                        size_t off = 0xc;
                        std::string rawStr = DecodeCp1252(data + off, len - off);
                        std::vector<std::string> strs;
                        std::string current;
                        for (char c : rawStr) {
                            if (c == '\0') {
                                strs.push_back(current);
                                current.clear();
                            } else {
                                current += c;
                            }
                        }
                        strs.push_back(current);
                        if (!strs.empty() && !strs[0].empty()) {
                            std::string p2;
                            for (size_t i = 1; i < strs.size(); ++i) {
                                if (!strs[i].empty()) {
                                    if (!p2.empty()) p2 += ",";
                                    p2 += strs[i];
                                }
                            }
                            bag->value = strs[0];
                            if (!p2.empty()) {
                                bag->value += " (" + p2 + ")";
                            }
                        }
                    } else {
                        size_t index = 14 + identifierSize;
                        if (shellPropertySheetListSize > 0 && index + shellPropertySheetListSize <= len) {
                            std::vector<uint8_t> propBytes(data + index, data + index + shellPropertySheetListSize);
                            bag->value = ExtractPropertyViewValue(propBytes);
                            // Look for extension blocks in propBytes if key "32" exists
                            try {
                                PropertyStore tmpStore(propBytes);
                                bool has32 = false;
                                for (const auto& sheet : tmpStore.sheets) {
                                    for (const auto& prop : sheet.properties) {
                                        if (std::get<0>(prop) == "32") { has32 = true; break; }
                                    }
                                }
                                if (has32) {
                                    // Regex-like scan for BEEF pattern in propBytes
                                    for (size_t i = 0; i + 7 < propBytes.size(); ++i) {
                                        if (propBytes[i+4] == 0xEF && propBytes[i+5] == 0xBE) {
                                            uint16_t exSize = ReadUInt16LE(propBytes.data(), i);
                                            if (exSize > 0 && i + exSize <= propBytes.size()) {
                                                std::vector<uint8_t> exBlock(propBytes.begin() + i, propBytes.begin() + i + exSize);
                                                size_t exOff = 0;
                                                auto block = ParseExtensionBlock(exBlock, exOff, codepage);
                                                if (block) bag->extensionBlocks.push_back(block);
                                            }
                                        }
                                    }
                                }
                            } catch (...) {}
                        } else {
                            // No property sheets - check for ZIP contents
                            if (len >= 0x28 && (data[0x28] == 0x2f ||
                                (data[0x24] == 0x4e && data[0x26] == 0x2f && data[0x28] == 0x41) ||
                                (data[0x1c] == 0x2f || (data[0x18] == 0x4e && data[0x1a] == 0x2f && data[0x1c] == 0x41)))) {
                                bag->friendlyName = "Variable: Zip file contents";
                                bag->value = "ZIP";
                            } else if (len >= 8 && data[4] == 0x41 && data[5] == 0x75 && data[6] == 0x67 && data[7] == 0x4D) {
                                // CD Burn (AugM)
                                bag->friendlyName = "Variable: CD Burn";
                                bag->value = "CD Burn";
                            } else {
                                bag->value = "!!! Unable to determine Value !!!";
                            }
                        }
                        index += shellPropertySheetListSize + 2;
                        if (index + 8 <= len) {
                            ParseExtensionBlocks(bag.get(), raw, index, len - index, codepage);
                        }
                    }
                    break;
                }

                // Zip file contents (dataSig == 0x00)
                if (dataSig == 0x00) {
                    if (len >= 0x28 && (data[0x28] == 0x2f ||
                        (data[0x24] == 0x4e && data[0x26] == 0x2f && data[0x28] == 0x41) ||
                        (data[0x1c] == 0x2f || (data[0x18] == 0x4e && data[0x1a] == 0x2f && data[0x1c] == 0x41)))) {
                        bag->friendlyName = "Variable: Zip file contents";
                        bag->value = "ZIP";
                    } else {
                        bag->value = "!!! Unable to determine Value !!!";
                    }
                    break;
                }
            }
            bag->friendlyName = "Root folder";
            if (len >= 20) {
                bag->value = GuidToString(data + 4);
            }
            break;
        }
        case 0x01: {
            // Hyper-V special case
            if (len >= 10 && data[8] == 0x3A && data[9] == 0x00) {
                bag->friendlyName = "Hyper-V storage volume";
                if (len >= 10) {
                    bag->driveLetter = ReadUnicodeStringFromBytes(data, 0x6, 4);
                }
                if (len >= 0x32) {
                    bag->value = ReadUnicodeStringFromBytes(data, 0x32, static_cast<uint32_t>(len - 0x32));
                }
                break;
            }
            uint32_t specialDataSig = ReadUInt32LE(data, 4);
            if (specialDataSig == 0x39de2184) {
                bag->friendlyName = "Control Panel Category";
                if (len > 8) {
                    switch (data[8]) {
                        case 0x00: bag->value = "All Control Panel Items"; break;
                        case 0x01: bag->value = "Appearance and Personalization"; break;
                        case 0x02: bag->value = "Hardware and Sound"; break;
                        case 0x03: bag->value = "Network and Internet"; break;
                        case 0x04: bag->value = "Sound, Speech and Audio Devices"; break;
                        case 0x05: bag->value = "System and Security"; break;
                        case 0x06: bag->value = "Clock, Language, and Region"; break;
                        case 0x07: bag->value = "Ease of Access"; break;
                        case 0x08: bag->value = "Programs"; break;
                        case 0x09: bag->value = "User Accounts"; break;
                        case 0x10: bag->value = "Security Center"; break;
                        case 0x11: bag->value = "Mobile PC"; break;
                        default: bag->value = "Unknown category! Category ID: " + std::to_string(data[8]); break;
                    }
                }
            } else {
                bag->friendlyName = "Volume";
                if (len >= 14) {
                    bag->value = ReadUnicodeStringFromBytes(data, 14, static_cast<uint32_t>((len - 14) * 2));
                }
                if (len >= 4) {
                    bag->driveLetter = std::string(1, static_cast<char>(data[3])) + ":";
                    if (bag->value.empty()) bag->value = bag->driveLetter;
                }
            }
            break;
        }
        case 0x71: {
            // Check for property view default
            if (len >= 10) {
                uint32_t dataSig = ReadUInt32LE(data, 6);
                if (dataSig == 0xbeebee00) {
                    bag->friendlyName = "Variable: Users property view";
                    uint16_t shellPropertySheetListSize = ReadUInt16LE(data, 10);
                    uint16_t identifierSize = ReadUInt16LE(data, 12);
                    size_t idx = 14 + identifierSize;
                    if (shellPropertySheetListSize > 0 && idx + shellPropertySheetListSize <= len) {
                        std::vector<uint8_t> propBytes(data + idx, data + idx + shellPropertySheetListSize);
                        bag->value = ExtractPropertyViewValue(propBytes);
                    } else {
                        bag->value = "!!! Unable to determine Value !!!";
                    }
                    idx += shellPropertySheetListSize + 2;
                    // Two GUIDs after property store
                    if (idx + 16 <= len) {
                        idx += 16; // skip first GUID
                    }
                    if (idx + 16 <= len) {
                        std::string guid2 = GuidToString(data + idx);
                        std::string name = GetKnownFolderName(guid2);
                        if (!name.empty()) bag->value = name;
                        idx += 16;
                    }
                    if (idx + 8 <= len) {
                        ParseExtensionBlocks(bag.get(), raw, idx, len - idx, codepage);
                    }
                    break;
                }
            }
            bag->friendlyName = "GUID: Control panel";
            if (len >= 18) {
                size_t guidOff = 14; // past size(2) + type(1) + unknown(1) + 10 zeros = 14
                if (len == 0x16) {
                    guidOff = 4;
                }
                if (guidOff + 16 <= len) {
                    bag->value = GuidToString(data + guidOff);
                }
                if (len > 32) {
                    size_t extOff = 0x1e;
                    if (extOff + 8 <= len) {
                        ParseExtensionBlocks(bag.get(), raw, extOff, len - extOff, codepage);
                    }
                }
            }
            break;
        }
        case 0x61: {
            bag->friendlyName = "URI";
            if (len >= 4) {
                size_t off = 2;
                off += 1; // type
                off += 1; // unknown
                bag->uriFlags = data[off];
                off += 1;
                uint16_t dataSize = ReadUInt16LE(data, off);
                off += 2;
                bag->userName = "";
                if (dataSize > 0 && off + 36 <= len) {
                    off += 4; // unknown
                    off += 4; // unknown
                    uint64_t ft = ReadUInt64LE(data, off);
                    if (ft > 0) bag->fileTime1 = FileTimeToTimeT(ft);
                    off += 8;
                    off += 4; // FF FF FF FF
                    off += 12; // 12 zeros
                    off += 4; // unknown
                    uint32_t strSize = ReadUInt32LE(data, off);
                    off += 4;
                    if (strSize > 0 && off + strSize <= len) {
                        bag->value = DecodeCp1252(data + off, strSize);
                        size_t np = bag->value.find('\0');
                        if (np != std::string::npos) bag->value = bag->value.substr(0, np);
                        off += strSize;
                    }
                    strSize = ReadUInt32LE(data, off);
                    off += 4;
                    if (strSize > 0 && off + strSize <= len) {
                        bag->userName = DecodeCp1252(data + off, strSize);
                        size_t np = bag->userName.find('\0');
                        if (np != std::string::npos) bag->userName = bag->userName.substr(0, np);
                        off += strSize;
                    }
                    strSize = ReadUInt32LE(data, off);
                    off += 4;
                    if (strSize > 0 && off + strSize <= len) {
                        off += strSize;
                    }
                    size_t len1 = 0;
                    while (off + len1 < len && data[off + len1] != 0) len1++;
                    bag->uri = DecodeCp1252(data + off, len1);
                    off += len1 + 1;
                } else {
                    // Fallback: Unicode string at offset 8
                    if (len >= 8) {
                        bag->value = ReadUnicodeStringFromBytes(data, 8, static_cast<uint32_t>((len - 8) * 2));
                        size_t np = bag->value.find('\0');
                        if (np != std::string::npos) bag->value = bag->value.substr(0, np);
                    }
                }
                if (off < len) {
                    uint16_t extraDataSize = ReadUInt16LE(data, off);
                    off += 2;
                    (void)extraDataSize; // extra data found
                }
            }
            break;
        }
        case 0x4C: {
            bag->friendlyName = "Sharepoint directory";
            if (len >= 0x1c + 2) {
                size_t off = 0x1c;
                uint16_t nameLen = ReadUInt16LE(data, off);
                off += 2;
                std::string name1;
                if (off + nameLen * 2 <= len) {
                    name1 = ReadUnicodeStringFromBytes(data, off, nameLen * 2);
                    off += nameLen * 2 + 2; // skip terminator
                }
                if (off + 2 <= len) {
                    uint16_t nameLen2 = ReadUInt16LE(data, off);
                    off += 2;
                    std::string name2;
                    if (off + nameLen2 * 2 <= len) {
                        name2 = ReadUnicodeStringFromBytes(data, off, nameLen2 * 2);
                    }
                    if (name2.empty()) name2 = "URL not specified";
                    bag->value = name1 + " (" + name2 + ")";
                }
            }
            break;
        }
        case 0xC3: {
            bag->friendlyName = "Network location";
            if (len >= 3) {
                size_t off = 2;
                bag->c3ClassType = data[off] & 0x70;
                off += 1;
                off += 1; // unknown1
                bag->c3Flags = data[off];
                off += 1;
                size_t nameLen = 0;
                while (off + nameLen < len && data[off + nameLen] != 0) nameLen++;
                if (nameLen > 0) {
                    bag->value = DecodeCp1252(data + off, nameLen);
                    off += nameLen;
                }
                while (off < len && data[off] == 0) off++;
            }
            break;
        }
        case 0x74:
        case 0x77: {
            bag->friendlyName = "Users Files Folder";
            if (len >= 8) {
                size_t off = 2; // past size
                off += 2; // past type + unknown
                uint16_t subSize = ReadUInt16LE(data, off);
                off += 2;
                // Signature check
                if (len >= 10) {
                    std::string sig74;
                    for (size_t i = 0; i < 4 && off + i < len; ++i) sig74 += static_cast<char>(data[off + i]);
                    // CF\0\0 ZIP check
                    if (sig74.substr(0, 2) == "CF" && len > 0x28) {
                        if (data[0x28] == 0x2f || (data[0x24] == 0x4e && data[0x26] == 0x2f && data[0x28] == 0x41)) {
                            bag->friendlyName = "Zip file contents";
                            size_t zipIdx = 84;
                            if (len > 0x14 && data[0x14] == 0x10) zipIdx = 60;
                            try {
                                uint16_t nameSize1 = ReadUInt16LE(data, zipIdx);
                                zipIdx += 4;
                                uint16_t nameSize2 = ReadUInt16LE(data, zipIdx);
                                zipIdx += 4;
                                if (nameSize1 > 0 && zipIdx + nameSize1 * 2 <= len) {
                                    bag->value = ReadUnicodeStringFromBytes(data, zipIdx, nameSize1 * 2);
                                    zipIdx += nameSize1 * 2 + 2;
                                }
                                if (nameSize2 > 0 && zipIdx + nameSize2 * 2 <= len) {
                                    zipIdx += nameSize2 * 2 + 2;
                                }
                                std::string rawdatestr;
                                if (len >= 0x24 + 40) {
                                    rawdatestr = ReadUnicodeStringFromBytes(data, 0x24, 40);
                                    size_t np = rawdatestr.find('\0');
                                    if (np != std::string::npos) rawdatestr = rawdatestr.substr(0, np);
                                }
                                if (!rawdatestr.empty()) {
                                    struct tm t = {};
                                    if (sscanf(rawdatestr.c_str(), "%d/%d/%d", &t.tm_mon, &t.tm_mday, &t.tm_year) == 3) {
                                        t.tm_year -= 1900;
                                        t.tm_mon -= 1;
                                        bag->lastAccessTime = timegm(&t);
                                    }
                                }
                            } catch (...) {}
                            break;
                        }
                    }
                    if (sig74 == "CFSF" && len >= 42) {
                        off += 4; // skip CFSF
                        uint16_t subShellSize = ReadUInt16LE(data, off);
                        off += 2; // subShellSize
                        uint8_t subClassType = data[off];
                        off += 1; // subClassType
                        off += 1; // skip unknown
                        uint32_t filesize = ReadUInt32LE(data, off);
                        off += 4; // filesize
                        if (off + 4 <= len) {
                            bag->lastModificationTime = DosDateTimeToTimeT(ReadUInt32LE(data, off));
                        }
                        off += 4;
                        off += 2; // skip file attribute flag
                        // Primary name (null-terminated ASCII)
                        size_t nameLen = 0;
                        while (off + nameLen < len && data[off + nameLen] != 0) nameLen++;
                        if (nameLen > 0) {
                            std::string primaryName = DecodeCp1252(data + off, nameLen);
                            bag->shortName = primaryName;
                            bag->value = primaryName;
                            off += nameLen;
                        }
                        while (off < len && data[off] == 0) off++;
                        // Delegate GUID (16 bytes)
                        if (off + 16 <= len) {
                            std::string delegateGuid = GuidToString(data + off);
                            // C# validates delegate GUID equals 5e591a74-df96-48d3-8d67-1733bcee28ba
                            // but throws exception if mismatch. We just validate and continue.
                            if (delegateGuid != "5e591a74-df96-48d3-8d67-1733bcee28ba") {
                                // Mismatched delegate GUID - log but continue
                            }
                            off += 16;
                        }
                        // Item identifier GUID (16 bytes)
                        if (off + 16 <= len) {
                            std::string itemGuid = GuidToString(data + off);
                            std::string itemName = GetKnownFolderName(itemGuid);
                            if (!itemName.empty()) {
                                bag->value = itemName;
                            }
                            off += 16;
                        }
                        // Extension blocks
                        if (off + 8 <= len) {
                            ParseExtensionBlocks(bag.get(), raw, off, len - off, codepage);
                        }
                        break;
                    }
                }
                // Fallback: simple ASCII string + extension blocks
                bag->value = ReadAsciiString(data, off);
                if (off < len && len > off + 4) {
                    ParseExtensionBlocks(bag.get(), raw, off, len - off, codepage);
                }
            }
            break;
        }
        case 0xae:
        case 0xaa:
        case 0x79: {
            bag->friendlyName = "Zip Contents";
            bag->value = "ZIP";
            break;
        }
        case 0x41: {
            bag->friendlyName = "Domain/Workgroup name";
            if (len >= 5) {
                size_t off = 5;
                std::string raw = DecodeCp1252(data + off, len - off);
                auto parts = SplitString(raw, '\0');
                if (!parts.empty()) bag->value = parts[0];
                if (parts.size() > 1) bag->networkDesc = parts[1];
            }
            break;
        }
        case 0x42: {
            bag->friendlyName = "Server UNC path";
            if (len >= 5) {
                size_t off = 5;
                std::string raw = DecodeCp1252(data + off, len - off);
                auto parts = SplitString(raw, '\0');
                if (!parts.empty()) bag->value = parts[0];
                if (parts.size() > 1) bag->networkDesc = parts[1];
            }
            break;
        }
        case 0x43: {
            bag->friendlyName = "Share UNC path";
            if (len >= 5) {
                size_t off = 5;
                std::string raw = DecodeCp1252(data + off, len - off);
                auto parts = SplitString(raw, '\0');
                if (!parts.empty()) bag->value = parts[0];
                if (parts.size() > 1) bag->networkDesc = parts[1];
            }
            break;
        }
        case 0x46: {
            bag->friendlyName = "Microsoft Windows Network";
            if (len >= 5) {
                size_t off = 5;
                std::string raw = DecodeCp1252(data + off, len - off);
                auto parts = SplitString(raw, '\0');
                if (!parts.empty()) bag->value = parts[0];
                if (parts.size() > 1) bag->networkDesc = parts[1];
            }
            break;
        }
        case 0x47: {
            bag->friendlyName = "Entire Network";
            if (len >= 5) {
                size_t off = 5;
                std::string raw = DecodeCp1252(data + off, len - off);
                auto parts = SplitString(raw, '\0');
                if (!parts.empty()) bag->value = parts[0];
                if (parts.size() > 1) bag->networkDesc = parts[1];
            }
            break;
        }
        case 0x49:
        case 0x4a:
        case 0x4b: {
            bag->friendlyName = "Network location";
            if (len >= 5) {
                size_t off = 5;
                std::string raw = DecodeCp1252(data + off, len - off);
                auto parts = SplitString(raw, '\0');
                if (!parts.empty()) bag->value = parts[0];
                if (parts.size() > 1) bag->networkDesc = parts[1];
            }
            break;
        }
        default: {
            bag->friendlyName = "Unknown (0x" + BytesToHex(&bag->type, 1) + ")";
            if (len >= 4) {
                size_t off = 3;
                bag->value = ReadAsciiString(data, off);
            }
            break;
        }
    }

    return bag;
}

void LnkFile::ParseExtensionBlocks(ShellBag* bag, const std::vector<uint8_t>& raw, size_t offset, size_t maxLen, int codepage) {
    size_t end = offset + maxLen;
    if (end > raw.size()) end = raw.size();

    while (offset + 8 <= end) {
        uint16_t chunkSize = ReadUInt16LE(raw.data(), offset);
        if (chunkSize == 0) break;
        if (chunkSize == 1) {
            offset += 2; // separator
            continue;
        }
        if (offset + chunkSize > end) break;

        std::vector<uint8_t> chunk(raw.begin() + offset, raw.begin() + offset + chunkSize);
        size_t chunkOff = 0;
        auto block = ParseExtensionBlock(chunk, chunkOff, codepage);
        if (block) {
            bag->extensionBlocks.push_back(block);
            // If Beef0004 has long name, update bag value
            if (auto b4 = std::dynamic_pointer_cast<Beef0004Block>(block)) {
                if (!b4->longName.empty()) {
                    bag->value = b4->longName;
                } else if (!b4->localisedName.empty()) {
                    bag->value = b4->localisedName;
                }
            }
        }
        offset += chunkSize;
    }
}

static std::string GetKnownFolderName(const std::string& guid) {
    static const std::unordered_map<std::string, std::string> kf = {
        {"de61d971-5ebc-4f02-a3a9-6c82895e5c04", "AddNewPrograms"},
        {"724ef170-a42d-4fef-9f26-b60e846fba4f", "AdminTools"},
        {"a305ce99-f527-492b-8b1a-7e76fa98d6e4", "AppDataLow"},
        {"3e645c70-997d-48c0-99f4-96692094207f", "AppUpdates"},
        {"a4115719-d62e-491d-aa7c-e74b8be3b067", "CDBurning"},
        {"df7266ac-9274-4867-8d55-3bd661de872d", "ChangeRemovePrograms"},
        {"d0384e7d-bac3-4797-8f14-cba229b392b5", "CommonAdminTools"},
        {"c1bae2d0-10df-4334-bedd-7aa20b227a9d", "CommonOEMLinks"},
        {"5399e694-6ce5-4d6c-8fce-1d8870fdcba0", "CommonPrograms"},
        {"dfdf76a2-c82a-4d63-906a-5644ac457385", "CommonStartMenu"},
        {"de92c1c7-837f-4f69-a3bb-86e631204a23", "CommonStartup"},
        {"0ac0837c-bbf8-452a-850d-79d08e667ca7", "CommonTemplates"},
        {"4bfefb45-347d-4006-a5be-ac0cb0567192", "Computer"},
        {"18989b1d-99b5-455b-841c-ab7c74e4ddfc", "Conflict"},
        {"2b0f765d-c0e9-4171-908e-08a611b84ff6", "Connections"},
        {"6d0def11-9ab4-49cd-9775-4616656a5a3b", "Contacts"},
        {"82a74aeb-aebc-465f-9bb9-8dd9b8c5e3b8", "ControlPanel"},
        {"2f6ce320-c219-11cf-9a87-00aa004ae835", "Cookies"},
        {"b4bfcc3a-db2c-424c-b029-7fe99a87c641", "Desktop"},
        {"5ce4a5e9-e4eb-479d-b89f-130c02886155", "Documents"},
        {"7b0db17d-9cd2-4a93-9733-46cc89022e7c", "Downloads"},
        {"fde440a0-2211-42c0-9346-6c6b59df291a", "Favorites"},
        {"374de290-123f-4565-9164-39c4925e467b", "Documents"},
        {"1777f761-68ad-4d8a-87bd-30b759fa33dd", "Favorites"},
        {"b94237e7-57ac-4347-9151-b08c6c32d1f7", "Games"},
        {"cac52c1a-b53d-4edc-92d7-6b2e8ac19434", "GameTasks"},
        {"054fae61-4dd8-4787-80b6-090220c4b700", "History"},
        {"4d9f7874-4e0c-4904-967b-40b0d20c3e4b", "InternetCache"},
        {"352481e8-33be-4251-ba85-6007caedcf9d", "Libraries"},
        {"bcbd3057-ca5c-4622-b42d-bc56db0ae516", "Links"},
        {"f1b32785-6fba-4fcf-9d55-7b8e7f157091", "LocalAppData"},
        {"2a00375e-224c-49de-b8d1-440df7ef3ddc", "LocalAppDataLow"},
        {"4bd8d571-6d19-48d3-be97-422220080e43", "Music"},
        {"bfb9d5e0-c6a9-404c-b2b2-ae6db6af4968", "NetHood"},
        {"c4abec8f-9954-43f2-8ac3-434e456d3862", "Network"},
        {"d20beec4-5ca8-4905-ae3b-bf251ea09b53", "OriginalImages"},
        {"2c36c0aa-5812-4b87-bfd0-4cd0dfb19b39", "PhotoAlbums"},
        {"69d2cf90-fc33-4fb7-9a0c-ebb0f0fcb43c", "Pictures"},
        {"33e28130-4e1e-4676-835a-98395c3bc3bb", "Pictures"},
        {"debc7b54-59f4-430b-9c53-4d2b3d6b3f1f", "Playlists"},
        {"76fc4e2d-d6ad-4519-a663-37bd56068185", "Printers"},
        {"9274bd8d-cfd1-41c3-b35e-b13f55a758f4", "PrintHood"},
        {"5e6c858f-0e22-4760-9afe-ea3317b67173", "Profile"},
        {"62ab5d82-fdc1-4dc3-a9dd-070d1d495d97", "ProgramData"},
        {"905e63b6-c1bf-494e-b29c-65b732d3d21a", "ProgramFiles"},
        {"f7f1ed05-9f6d-47a2-aaae-29d317c6f066", "ProgramFilesCommon"},
        {"6365d5a7-0f0d-45e5-87f0-0da27894f6c2", "ProgramFilesCommonX64"},
        {"de974d24-d9c9-4d3e-bf91-f4455120b917", "ProgramFilesCommonX86"},
        {"2cef7f55-9696-44b5-92a8-4c2c4b2e2e2e", "ProgramFilesX64"},
        {"7c5a40ef-a0fb-4bfc-874a-c0f2e0b9fa8e", "ProgramFilesX86"},
        {"a77f5d77-2e2b-44c3-a6a2-aba601054a51", "Programs"},
        {"df7266ac-9274-4867-8d55-3bd661de872d", "Public"},
        {"c4abec8f-9954-43f2-8ac3-434e456d3862", "PublicDesktop"},
        {"b4e6f1e5-6e6c-4c7b-8e1e-1a6e3e6a2a6a", "PublicDocuments"},
        {"3214fab5-9757-4298-bb61-92a9de7c2c7c", "PublicDownloads"},
        {"56c1e5e4-5ea4-4c6f-8f1a-3e6a3e6a2a6a", "PublicGameTasks"},
        {"0482af6c-08c4-423c-8e3d-3e6a3e6a2a6a", "PublicLibraries"},
        {"b6ebfb86-6907-413c-9af7-4fc2ab07f6cc", "PublicMusic"},
        {"2400183a-6185-49fb-a2d8-4a392a602ba3", "PublicPictures"},
        {"e555ab60-153b-4d17-9f04-a5fe86fc4c5e", "PublicRingtones"},
        {"c4900540-2379-4c75-844b-64e6faf8716b", "PublicVideos"},
        {"3dfdf28f-fdfb-4b1f-a5c7-eb3e5e8e5e8e", "QuickLaunch"},
        {"bfb9d5e0-c6a9-404c-b2b2-ae6db6af4968", "Recent"},
        {"1a6fdfa2-3c1c-4c6f-8f1a-3e6a3e6a2a6a", "RecordedTV"},
        {"8ad10c31-2adb-4296-a8f7-e4701232c972", "RecycleBin"},
        {"9e3995ab-1a6a-4e6b-8f1a-3e6a3e6a2a6a", "Ringtones"},
        {"c870044b-f49e-4126-a9c3-b52a1ff411e8", "RoamingAppData"},
        {"b7534046-3ecb-4c18-be4e-64cd4cb7d6ac", "SavedGames"},
        {"4c5c32ff-bb9d-43b0-b5b4-2d72e54eaaa4", "SavedSearches"},
        {"ee32e446-31ca-4aba-814f-a5ebd2fd6d5e", "SEARCH_CSC"},
        {"98ec0e18-2098-4d44-8644-66979315a281", "SEARCH_MAPI"},
        {"190337d1-b8ca-4121-a639-6d472d16972a", "SearchHome"},
        {"4d8f9a3b-5f2e-4c6f-8f1a-3e6a3e6a2a6a", "SendTo"},
        {"8983036c-27c0-404b-908f-85c8f5e3f923", "SidebarDefaultParts"},
        {"7b396e54-9ec5-430c-bea0-a3ad65de2b5e", "SidebarParts"},
        {"a52bba46-e9e1-435f-b3d9-28daa648c0f6", "StartMenu"},
        {"df7266ac-9274-4867-8d55-3bd661de872d", "StartMenuAllPrograms"},
        {"625b53c3-ab48-4ec1-ba1f-a1ef4146fc19", "Startup"},
        {"43668bf8-c14e-49b2-97c9-9747f1d8d8e3", "SyncManager"},
        {"289a9a43-be44-4057-a41b-587a76d7e7f9", "SyncResults"},
        {"0f214138-b1d3-4a90-bba9-27cbc0c5389a", "SyncSetup"},
        {"1ac14e77-02e7-4e5d-b744-2eb1ae5198b7", "System"},
        {"d65231b0-b2f1-4857-a4ce-a8e7c6ea7d27", "SystemX86"},
        {"a63293e8-664e-48db-a079-df759e0509f7", "Templates"},
        {"b4bfcc3a-db2c-424c-b029-7fe99a87c641", "UserPinned"},
        {"18989b1d-99b5-455b-841c-ab7c74e4ddfc", "UserProfiles"},
        {"0762d272-c50a-4bb0-a382-697dcd729b80", "UsersFiles"},
        {"5cd7aee2-2219-4a67-b85d-6c9ce15660cb", "UsersLibraries"},
        {"f38bf404-1d43-42f2-9305-67de0b28fc23", "Windows"},
        {"ca8d6317-3a5e-4c6f-8f1a-3e6a3e6a2a6a", "WindowsBurn"},
    };
    auto it = kf.find(guid);
    if (it != kf.end()) return it->second;
    return "";
}

static std::time_t GetDateTimeFromGuid(const uint8_t* guidBytes) {
    // .NET Guid timestamp extraction (version 1 GUID)
    // Byte 7 contains version in upper nibble; mask it out
    uint8_t temp[8];
    std::memcpy(temp, guidBytes, 8);
    temp[7] &= 0x0F;

    // Read as little-endian int64 (100-nanosecond intervals since 1582-10-15)
    int64_t timestamp = static_cast<int64_t>(ReadUInt64LE(temp, 0));

    // Gregorian calendar start in .NET ticks
    const int64_t GREGORIAN_TICKS = 499163040000000000LL;
    // Unix epoch in .NET ticks
    const int64_t UNIX_TICKS = 621355968000000000LL;

    int64_t ticks = timestamp + GREGORIAN_TICKS;
    int64_t unixSeconds = (ticks - UNIX_TICKS) / 10000000;

    if (unixSeconds < 0 || unixSeconds > 4102444800LL) {
        return 0;
    }
    return static_cast<std::time_t>(unixSeconds);
}

static std::time_t DosDateTimeToTimeT(uint32_t dosDateTime) {
    // DOS date/time format:
    // Bits 0-4: seconds/2
    // Bits 5-10: minutes
    // Bits 11-15: hours
    // Bits 16-20: day
    // Bits 21-24: month
    // Bits 25-31: year since 1980
    if (dosDateTime == 0 || dosDateTime == 0xFFFFFFFF) return 0;
    int sec = (dosDateTime & 0x1F) * 2;
    int min = (dosDateTime >> 5) & 0x3F;
    int hour = (dosDateTime >> 11) & 0x1F;
    int day = (dosDateTime >> 16) & 0x1F;
    int mon = ((dosDateTime >> 21) & 0x0F);
    int year = ((dosDateTime >> 25) & 0x7F) + 1980;
    if (mon < 1 || mon > 12 || day < 1 || day > 31) return 0;
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;
    return timegm(&t);
}

std::shared_ptr<ExtensionBlock> LnkFile::ParseExtensionBlock(const std::vector<uint8_t>& raw, size_t& offset, int codepage) {
    if (offset + 8 > raw.size()) return nullptr;

    const uint8_t* data = raw.data();
    uint16_t blockSize = ReadUInt16LE(data, offset);
    uint16_t version = ReadUInt16LE(data, offset + 2);
    uint32_t signature = ReadUInt32LE(data, offset + 4);

    if (blockSize == 0 || offset + blockSize > raw.size()) return nullptr;

    switch (signature) {
        case 0xBEEF0003: {
            auto block = std::make_shared<Beef0003Block>();
            block->signature = signature;
            if (offset + 20 <= raw.size()) {
                block->guid1 = GuidToString(data + offset + 8);
                block->guid1Folder = GetKnownFolderName(block->guid1);
            }
            offset += blockSize;
            return block;
        }
        case 0xBEEF0026: {
            auto block = std::make_shared<Beef0026Block>();
            block->signature = signature;
            block->version = version;
            if (offset + 9 <= offset + blockSize) {
                uint8_t flag = data[offset + 8];
                if (flag == 0x10 || flag == 0x11 || flag == 0x12 || flag == 0x31 || flag == 0x34) {
                    // FILETIME trio path
                    if (offset + 20 <= offset + blockSize) {
                        uint64_t ft1 = ReadUInt64LE(data, offset + 12);
                        if (ft1 != 0) block->createdOn = FileTimeToTimeT(ft1);
                    }
                    if (offset + 28 <= offset + blockSize) {
                        uint64_t ft2 = ReadUInt64LE(data, offset + 20);
                        if (ft2 != 0) block->lastModified = FileTimeToTimeT(ft2);
                    }
                    if (offset + 36 <= offset + blockSize) {
                        uint64_t ft3 = ReadUInt64LE(data, offset + 28);
                        if (ft3 != 0) block->lastAccessed = FileTimeToTimeT(ft3);
                    }
                } else {
                    // PropertyStore path
                    if (offset + 10 <= offset + blockSize) {
                        uint16_t shellPropertySheetListSize = ReadUInt16LE(data, offset + 8);
                        if (shellPropertySheetListSize > 0 && shellPropertySheetListSize <= blockSize - 8) {
                            std::vector<uint8_t> propBytes(data + offset + 8,
                                                            data + offset + 8 + shellPropertySheetListSize);
                            try {
                                PropertyStore ps(propBytes);
                                for (const auto& nsheet : ps.sheets) {
                                    for (const auto& nprop : nsheet.properties) {
                                        block->propertyStore.emplace_back(std::get<0>(nprop), std::get<2>(nprop));
                                    }
                                }
                            } catch (...) {
                                // Ignore parse errors
                            }
                        }
                    }
                    if (offset + blockSize >= 4) {
                        block->versionOffset = static_cast<int16_t>(ReadUInt16LE(data, offset + blockSize - 4));
                    }
                }
            }
            offset += blockSize;
            return block;
        }
        case 0xBEEF0004: {
            auto block = std::make_shared<Beef0004Block>();
            block->signature = signature;
            block->version = version;
            size_t off = offset + 8;
            if (off + 4 <= raw.size()) {
                block->createdOnTime = DosDateTimeToTimeT(ReadUInt32LE(data, off));
            }
            if (off + 4 <= raw.size()) {
                block->lastAccessTime = DosDateTimeToTimeT(ReadUInt32LE(data, off + 4));
            }
            // Identifier at bytes 16-17
            if (offset + 18 <= raw.size()) {
                block->identifier = static_cast<int16_t>(ReadUInt16LE(data, offset + 16));
            }
            off = offset + 18;
            if (version >= 7 && off + 18 <= offset + blockSize) {
                off += 2; // skip empty 2
                // MFT info: 8 bytes
                uint32_t entryLow = ReadUInt32LE(data, off);
                uint16_t entryHigh = ReadUInt16LE(data, off + 4);
                uint16_t seqNum = ReadUInt16LE(data, off + 6);
                uint64_t entryIndex = entryLow;
                if (entryHigh != 0) {
                    entryIndex += (static_cast<uint64_t>(entryHigh) << 32);
                }
                block->mftInfo.mftEntryNumber = entryIndex;
                if (seqNum != 0) {
                    block->mftInfo.mftSequenceNumber = seqNum;
                }
                // Determine file system hint
                if (block->mftInfo.mftEntryNumber.has_value() && block->mftInfo.mftSequenceNumber.has_value()) {
                    if (block->mftInfo.mftEntryNumber.value() > 0 && block->mftInfo.mftSequenceNumber.value() > 0) {
                        block->mftInfo.note = "NTFS";
                    }
                } else if (block->mftInfo.mftEntryNumber.has_value() && !block->mftInfo.mftSequenceNumber.has_value()) {
                    if (block->lastAccessTime.has_value() && block->lastAccessTime.value() != 0) {
                        struct tm* t = gmtime(&(*block->lastAccessTime));
                        if (t && t->tm_min == 0 && t->tm_sec == 0) {
                            block->mftInfo.note = "FAT";
                        } else {
                            block->mftInfo.note = "exFAT";
                        }
                    }
                }
                off += 8;  // mft data
                off += 8;  // unknown
            }
            if (version >= 3) {
                off += 2;
            }
            if (version >= 9) {
                off += 4;
            }
            if (version >= 8) {
                off += 4;
            }
            // Unicode strings at end, last 2 bytes are VersionOffset
            size_t stringEnd = offset + blockSize - 2;
            if (stringEnd > off && stringEnd <= raw.size()) {
                size_t stringLen = stringEnd - off;
                std::vector<std::string> stringPieces;
                std::string current;
                for (size_t i = 0; i < stringLen / 2; ++i) {
                    uint16_t ch = ReadUInt16LE(data, off + i * 2);
                    if (ch == 0) {
                        stringPieces.push_back(current);
                        current.clear();
                        continue;
                    }
                    if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < stringLen / 2) {
                        uint16_t low = ReadUInt16LE(data, off + (i + 1) * 2);
                        if (low >= 0xDC00 && low <= 0xDFFF) {
                            uint32_t codepoint = 0x10000 + ((ch - 0xD800) << 10) + (low - 0xDC00);
                            AppendUtf8(current, codepoint);
                            i++;
                            continue;
                        }
                    }
                    AppendUtf8(current, ch);
                }
                if (!current.empty()) {
                    stringPieces.push_back(current);
                }
                if (stringPieces.size() >= 1) {
                    block->longName = stringPieces[0];
                }
                if (stringPieces.size() > 1) {
                    block->localisedName = stringPieces[1];
                }
            }
            offset += blockSize;
            return block;
        }
        case 0xBEEF001A: {
            auto block = std::make_shared<Beef001aBlock>();
            block->signature = signature;
            // C# reads at offset 10 (skips 2 unknown bytes after signature)
            if (offset + 10 < offset + blockSize && blockSize > 10) {
                size_t off = offset + 10;
                // Leave last 2 bytes for VersionOffset
                size_t maxChars = (offset + blockSize - off - 2) / 2;
                block->fileDocumentTypeString = ReadUnicodeNullTerminated(data, off, maxChars);
            }
            offset += blockSize;
            return block;
        }
        case 0xBEEF0025: {
            auto block = std::make_shared<Beef0025Block>();
            block->signature = signature;
            if (offset + 16 <= raw.size()) {
                uint64_t ft1 = ReadUInt64LE(data, offset + 8);
                uint64_t ft2 = ReadUInt64LE(data, offset + 16);
                if (ft1 != 0) block->fileTime1 = FileTimeToTimeT(ft1);
                if (ft2 != 0) block->fileTime2 = FileTimeToTimeT(ft2);
            }
            offset += blockSize;
            return block;
        }
        case 0xBEEF0005: {
            auto block = std::make_shared<Beef0005Block>();
            block->signature = signature;
            offset += blockSize;
            return block;
        }
        default: {
            // Unknown BEEF block — preserve raw bytes like C# BeefUnknown
            auto block = std::make_shared<BeefUnknownBlock>();
            block->signature = signature;
            block->version = version;
            if (offset + blockSize <= raw.size()) {
                block->rawBytes.assign(data + offset, data + offset + blockSize);
            }
            offset += blockSize;
            return block;
        }
    }
}

void LnkFile::ParseLinkInfo(const std::vector<uint8_t>& raw, size_t& index) {
    if (index + 4 > raw.size()) return;

    linkInfo = std::make_shared<LinkInfo>();
    const uint8_t* data = raw.data();

    linkInfo->size = ReadUInt32LE(data, index);
    if (linkInfo->size == 0 || index + linkInfo->size > raw.size()) {
        linkInfo.reset();
        return;
    }

    linkInfo->headerSize = ReadUInt32LE(data, index + 4);
    linkInfo->flags = ReadUInt32LE(data, index + 8);
    linkInfo->volumeIdOffset = ReadUInt32LE(data, index + 12);
    linkInfo->localBasePathOffset = ReadUInt32LE(data, index + 16);
    linkInfo->commonNetworkRelativeLinkOffset = ReadUInt32LE(data, index + 20);
    linkInfo->commonPathSuffixOffset = ReadUInt32LE(data, index + 24);

    bool unicode = linkInfo->headerSize >= 0x24;
    if (unicode) {
        linkInfo->localBasePathOffsetUnicode = ReadUInt32LE(data, index + 28);
        linkInfo->commonPathSuffixOffsetUnicode = ReadUInt32LE(data, index + 32);
    }

    // Volume info
    if (linkInfo->HasVolumeID() && linkInfo->volumeIdOffset > 0) {
        size_t volOff = index + linkInfo->volumeIdOffset;
        if (volOff + 16 <= raw.size()) {
            auto vi = std::make_shared<VolumeInfo>();
            vi->size = ReadUInt32LE(data, volOff);
            vi->driveType = static_cast<VolumeInfo::DriveType>(ReadUInt32LE(data, volOff + 4));
            uint32_t serial = ReadUInt32LE(data, volOff + 8);
            std::ostringstream oss;
            oss << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << serial;
            vi->volumeSerialNumber = oss.str();
            vi->volumeLabelOffset = ReadUInt32LE(data, volOff + 12);

            size_t lblOff = volOff + vi->volumeLabelOffset;
            if (vi->volumeLabelOffset > 16 && lblOff < raw.size()) {
                // Unicode label (Vista+)
                size_t maxBytes = raw.size() - lblOff;
                size_t maxChars = maxBytes / 2;
                std::string lbl;
                for (size_t i = 0; i < maxChars; ++i) {
                    uint16_t ch = ReadUInt16LE(data, lblOff + i * 2);
                    if (ch == 0) break;
                    if (ch < 0x80) lbl += static_cast<char>(ch);
                    else if (ch < 0x800) {
                        lbl += static_cast<char>(0xC0 | (ch >> 6));
                        lbl += static_cast<char>(0x80 | (ch & 0x3F));
                    } else {
                        lbl += static_cast<char>(0xE0 | (ch >> 12));
                        lbl += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                        lbl += static_cast<char>(0x80 | (ch & 0x3F));
                    }
                }
                vi->volumeLabel = lbl;
            } else if (volOff + 16 < raw.size()) {
                // Inline or offset ASCII label
                size_t maxLen = raw.size() - (volOff + 16);
                size_t len = 0;
                for (size_t i = 0; i < maxLen && data[volOff + 16 + i] != 0; ++i) len++;
                vi->volumeLabel = DecodeCp1252(data + volOff + 16, len);
            }
            linkInfo->volumeInfo = vi;
        }
    }

    // Network share info
    if (linkInfo->HasNetworkShare() && linkInfo->commonNetworkRelativeLinkOffset > 0) {
        size_t netOff = index + linkInfo->commonNetworkRelativeLinkOffset;
        if (netOff + 20 <= raw.size()) {
            auto ni = std::make_shared<NetworkShareInfo>();
            ni->size = ReadUInt32LE(data, netOff);
            ni->flags = ReadUInt32LE(data, netOff + 4);
            ni->netNameOffset = ReadUInt32LE(data, netOff + 8);
            ni->deviceNameOffset = ReadUInt32LE(data, netOff + 12);
            ni->networkProviderType = ReadUInt32LE(data, netOff + 16);

            if (ni->netNameOffset > 20 && netOff + 28 <= raw.size()) {
                // Unicode network share name (Vista+)
                uint32_t uniNetNameOffset = ReadUInt32LE(data, netOff + 20);
                uint32_t uniDevNameOffset = ReadUInt32LE(data, netOff + 24);
                if (netOff + uniNetNameOffset < raw.size()) {
                    size_t nameOff = netOff + uniNetNameOffset;
                    size_t maxBytes = raw.size() - nameOff;
                    size_t maxChars = maxBytes / 2;
                    std::string name;
                    for (size_t i = 0; i < maxChars; ++i) {
                        uint16_t ch = ReadUInt16LE(data, nameOff + i * 2);
                        if (ch == 0) break;
                        if (ch < 0x80) name += static_cast<char>(ch);
                        else if (ch < 0x800) {
                            name += static_cast<char>(0xC0 | (ch >> 6));
                            name += static_cast<char>(0x80 | (ch & 0x3F));
                        } else {
                            name += static_cast<char>(0xE0 | (ch >> 12));
                            name += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                            name += static_cast<char>(0x80 | (ch & 0x3F));
                        }
                    }
                    ni->networkShareName = name;
                }
                if (uniDevNameOffset > 0 && netOff + ni->deviceNameOffset < raw.size()) {
                    size_t devOff = netOff + ni->deviceNameOffset;
                    size_t maxBytes = raw.size() - devOff;
                    size_t maxChars = maxBytes / 2;
                    std::string dev;
                    for (size_t i = 0; i < maxChars; ++i) {
                        uint16_t ch = ReadUInt16LE(data, devOff + i * 2);
                        if (ch == 0) break;
                        if (ch < 0x80) dev += static_cast<char>(ch);
                        else if (ch < 0x800) {
                            dev += static_cast<char>(0xC0 | (ch >> 6));
                            dev += static_cast<char>(0x80 | (ch & 0x3F));
                        } else {
                            dev += static_cast<char>(0xE0 | (ch >> 12));
                            dev += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                            dev += static_cast<char>(0x80 | (ch & 0x3F));
                        }
                    }
                    ni->deviceName = dev;
                }
            } else {
                // ASCII/codepage network share name
                if (ni->netNameOffset > 0 && netOff + ni->netNameOffset < raw.size()) {
                    size_t nameOff = netOff + ni->netNameOffset;
                    size_t maxLen = raw.size() - nameOff;
                    size_t len = 0;
                    for (size_t i = 0; i < maxLen && data[nameOff + i] != 0; ++i) len++;
                    ni->networkShareName = DecodeCp1252(data + nameOff, len);
                }
                if (ni->deviceNameOffset > 0 && netOff + ni->deviceNameOffset < raw.size()) {
                    size_t devOff = netOff + ni->deviceNameOffset;
                    size_t maxLen = raw.size() - devOff;
                    size_t len = 0;
                    for (size_t i = 0; i < maxLen && data[devOff + i] != 0; ++i) len++;
                    ni->deviceName = DecodeCp1252(data + devOff, len);
                }
            }
            linkInfo->networkShareInfo = ni;
        }
    }

    // Local base path
    if (linkInfo->localBasePathOffset > 0) {
        size_t pathOff = index + linkInfo->localBasePathOffset;
        if (pathOff < raw.size()) {
            size_t maxLen = raw.size() - pathOff;
            size_t len = 0;
            for (size_t i = 0; i < maxLen && data[pathOff + i] != 0; ++i) len++;
            linkInfo->localBasePath = DecodeCp1252(data + pathOff, len);
        }
    }

    // Common path suffix
    if (linkInfo->commonPathSuffixOffset > 0) {
        size_t pathOff = index + linkInfo->commonPathSuffixOffset;
        if (pathOff < raw.size()) {
            size_t maxLen = raw.size() - pathOff;
            size_t len = 0;
            for (size_t i = 0; i < maxLen && data[pathOff + i] != 0; ++i) len++;
            linkInfo->commonPathSuffix = DecodeCp1252(data + pathOff, len);
        }
    }

    // Unicode paths (Vista+)
    if (unicode) {
        if (linkInfo->localBasePathOffsetUnicode > 0) {
            size_t pathOff = index + linkInfo->localBasePathOffsetUnicode;
            if (pathOff + 2 <= raw.size()) {
                size_t maxChars = (raw.size() - pathOff) / 2;
                std::string path;
                for (size_t i = 0; i < maxChars; ++i) {
                    uint16_t ch = ReadUInt16LE(data, pathOff + i * 2);
                    if (ch == 0) break;
                    if (ch < 0x80) path += static_cast<char>(ch);
                    else if (ch < 0x800) {
                        path += static_cast<char>(0xC0 | (ch >> 6));
                        path += static_cast<char>(0x80 | (ch & 0x3F));
                    } else {
                        path += static_cast<char>(0xE0 | (ch >> 12));
                        path += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                        path += static_cast<char>(0x80 | (ch & 0x3F));
                    }
                }
                linkInfo->localBasePathUnicode = path;
            }
        }
        if (linkInfo->commonPathSuffixOffsetUnicode > 0) {
            size_t pathOff = index + linkInfo->commonPathSuffixOffsetUnicode;
            if (pathOff + 2 <= raw.size()) {
                size_t maxChars = (raw.size() - pathOff) / 2;
                std::string path;
                for (size_t i = 0; i < maxChars; ++i) {
                    uint16_t ch = ReadUInt16LE(data, pathOff + i * 2);
                    if (ch == 0) break;
                    if (ch < 0x80) path += static_cast<char>(ch);
                    else if (ch < 0x800) {
                        path += static_cast<char>(0xC0 | (ch >> 6));
                        path += static_cast<char>(0x80 | (ch & 0x3F));
                    } else {
                        path += static_cast<char>(0xE0 | (ch >> 12));
                        path += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                        path += static_cast<char>(0x80 | (ch & 0x3F));
                    }
                }
                linkInfo->commonPathSuffixUnicode = path;
            }
        }
    }

    index += linkInfo->size;
}

void LnkFile::ParseStringData(const std::vector<uint8_t>& raw, size_t& index) {
    bool unicode = header.HasFlag(LnkHeader::IsUnicode);
    const uint8_t* data = raw.data();

    if (header.HasFlag(LnkHeader::HasName)) {
        if (index + 2 > raw.size()) return;
        uint16_t count = ReadUInt16LE(data, index);
        index += 2;
        if (unicode) {
            name = ReadUnicodeString(data, index, count);
        } else {
            name = ReadAsciiStringCount(data, index, count);
        }
    }

    if (header.HasFlag(LnkHeader::HasRelativePath)) {
        if (index + 2 > raw.size()) return;
        uint16_t count = ReadUInt16LE(data, index);
        index += 2;
        if (unicode) {
            relativePath = ReadUnicodeString(data, index, count);
        } else {
            relativePath = ReadAsciiStringCount(data, index, count);
        }
    }

    if (header.HasFlag(LnkHeader::HasWorkingDir)) {
        if (index + 2 > raw.size()) return;
        uint16_t count = ReadUInt16LE(data, index);
        index += 2;
        if (unicode) {
            workingDirectory = ReadUnicodeString(data, index, count);
        } else {
            workingDirectory = ReadAsciiStringCount(data, index, count);
        }
    }

    if (header.HasFlag(LnkHeader::HasArguments)) {
        if (index + 2 > raw.size()) return;
        uint16_t count = ReadUInt16LE(data, index);
        index += 2;
        if (unicode) {
            arguments = ReadUnicodeString(data, index, count);
        } else {
            arguments = ReadAsciiStringCount(data, index, count);
        }
    }

    if (header.HasFlag(LnkHeader::HasIconLocation)) {
        if (index + 2 > raw.size()) return;
        uint16_t count = ReadUInt16LE(data, index);
        index += 2;
        if (unicode) {
            iconLocation = ReadUnicodeString(data, index, count);
        } else {
            iconLocation = ReadAsciiStringCount(data, index, count);
        }
    }
}

void LnkFile::ParseExtraData(const std::vector<uint8_t>& raw, size_t& index) {
    const uint8_t* data = raw.data();

    while (index + 8 <= raw.size()) {
        uint32_t size = ReadUInt32LE(data, index);
        uint32_t sig = ReadUInt32LE(data, index + 4);

        if (size == 0 && sig == 0) {
            // Terminal block
            break;
        }

        if (size == 0 || static_cast<uint32_t>(size) >= raw.size()) {
            break;
        }
        if (index + size > raw.size()) {
            size = static_cast<uint32_t>(raw.size() - index);
        }

        switch (sig) {
            case 0xA0000002: { // ConsoleDataBlock
                auto block = std::make_shared<ConsoleDataBlock>();
                block->size = size;
                block->signature = sig;
                if (size >= 0x40) {
                    block->fillAttributes = ReadUInt16LE(data, index + 8);
                    block->popupFillAttributes = ReadUInt16LE(data, index + 10);
                    block->screenWidthBufferSize = ReadUInt16LE(data, index + 12);
                    block->screenHeightBufferSize = ReadUInt16LE(data, index + 14);
                    block->windowWidth = ReadUInt16LE(data, index + 16);
                    block->windowHeight = ReadUInt16LE(data, index + 18);
                    block->windowOriginX = ReadUInt16LE(data, index + 20);
                    block->windowOriginY = ReadUInt16LE(data, index + 22);
                    block->reserved0 = static_cast<int32_t>(ReadUInt32LE(data, index + 24));
                    block->reserved1 = static_cast<int32_t>(ReadUInt32LE(data, index + 28));
                    block->fontSize = ReadUInt16LE(data, index + 34);
                    block->fontFamily = ReadUInt32LE(data, index + 36);
                    block->fontWeight = ReadUInt16LE(data, index + 40);
                    block->isBold = block->fontWeight >= 700;
                    size_t off = index + 44;
                    block->faceName = ReadUnicodeNullTerminated(data, off, 32);
                    block->cursorSize = ReadUInt32LE(data, index + 108);
                    block->fullScreen = ReadUInt32LE(data, index + 112);
                    block->quickEdit = ReadUInt32LE(data, index + 116);
                    block->insertMode = ReadUInt32LE(data, index + 120);
                    block->autoPosition = ReadUInt32LE(data, index + 124);
                    block->historyBufferSize = ReadUInt32LE(data, index + 128);
                    block->numberOfHistoryBuffers = ReadUInt32LE(data, index + 132);
                    block->historyNoDup = ReadUInt32LE(data, index + 136);
                    if (size >= 200) {
                        for (int i = 0; i < 8; ++i) {
                            block->colorTable.push_back(ReadUInt32LE(data, index + 140 + i * 8));
                        }
                    }
                }
                extraBlocks.push_back(block);
                break;
            }
            case 0xA0000004: { // ConsoleFEDataBlock
                auto block = std::make_shared<ConsoleFEDataBlock>();
                block->size = size;
                block->signature = sig;
                if (size >= 12) {
                    block->codePage = ReadUInt32LE(data, index + 8);
                }
                extraBlocks.push_back(block);
                break;
            }
            case 0xA0000006: { // DarwinDataBlock
                auto block = std::make_shared<DarwinDataBlock>();
                block->size = size;
                block->signature = sig;
                if (size >= 0x314) {
                    block->applicationIdentifierAscii = DecodeCp1252(data + index + 8, 260);
                    size_t np = block->applicationIdentifierAscii.find('\0');
                    if (np != std::string::npos) block->applicationIdentifierAscii = block->applicationIdentifierAscii.substr(0, np);
                    size_t off = index + 268;
                    block->applicationIdentifierUnicode = ReadUnicodeNullTerminated(data, off, 260);
                    char sepChar = '>';
                    if (block->applicationIdentifierAscii.find('<') != std::string::npos) sepChar = '<';
                    auto segs = SplitString(block->applicationIdentifierAscii, sepChar);
                    if (!segs.empty()) {
                        auto left = segs[0];
                        if (left.length() >= 20) {
                            block->productCode = DecodeDarwinToGuid(left.substr(0, 20));
                            block->featureName = left.length() > 20 ? left.substr(20) : "(None)";
                        }
                        if (segs.size() > 1) {
                            block->componentId = DecodeDarwinToGuid(segs[1]);
                        } else {
                            block->componentId = "(None)";
                        }
                    }
                }
                extraBlocks.push_back(block);
                break;
            }
            case 0xA0000001: { // EnvironmentVariableDataBlock
                auto block = std::make_shared<EnvironmentVariableDataBlock>();
                block->size = size;
                block->signature = sig;
                if (size >= 0x20A) {
                    block->environmentVariablesAscii = DecodeCp1252(data + index + 8, 260);
                    size_t np = block->environmentVariablesAscii.find('\0');
                    if (np != std::string::npos) block->environmentVariablesAscii = block->environmentVariablesAscii.substr(0, np);
                    size_t off = index + 268;
                    block->environmentVariablesUnicode = ReadUnicodeNullTerminated(data, off, 260);
                }
                extraBlocks.push_back(block);
                break;
            }
            case 0xA0000007: { // IconEnvironmentDataBlock
                auto block = std::make_shared<IconEnvironmentDataBlock>();
                block->size = size;
                block->signature = sig;
                if (size >= 0x20A) {
                    block->iconPathAscii = DecodeCp1252(data + index + 8, 260);
                    size_t np = block->iconPathAscii.find('\0');
                    if (np != std::string::npos) block->iconPathAscii = block->iconPathAscii.substr(0, np);
                    size_t off = index + 268;
                    block->iconPathUni = ReadUnicodeNullTerminated(data, off, 260);
                }
                extraBlocks.push_back(block);
                break;
            }
            case 0xA000000B: { // KnownFolderDataBlock
                auto block = std::make_shared<KnownFolderDataBlock>();
                block->size = size;
                block->signature = sig;
                if (size >= 28) {
                    block->knownFolderId = GuidToString(data + index + 8);
                    block->offset = ReadUInt32LE(data, index + 24);
                    block->knownFolderName = GetKnownFolderName(block->knownFolderId);
                }
                extraBlocks.push_back(block);
                break;
            }
            case 0xA0000009: { // PropertyStoreDataBlock
                auto block = std::make_shared<PropertyStoreDataBlock>();
                block->size = size;
                block->signature = sig;
                if (size > 8) {
                    std::vector<uint8_t> storeBytes(data + index + 8, data + index + size);
                    PropertyStore ps(storeBytes);
                    block->sheets = std::move(ps.sheets);
                }
                extraBlocks.push_back(block);
                break;
            }
            case 0xA0000008: { // ShimDataBlock
                auto block = std::make_shared<ShimDataBlock>();
                block->size = size;
                block->signature = sig;
                if (size > 8) {
                    size_t off = index + 8;
                    block->layerName = ReadUnicodeNullTerminated(data, off, (size - 8) / 2);
                }
                extraBlocks.push_back(block);
                break;
            }
            case 0xA0000005: { // SpecialFolderDataBlock
                auto block = std::make_shared<SpecialFolderDataBlock>();
                block->size = size;
                block->signature = sig;
                if (size >= 16) {
                    block->specialFolderId = ReadUInt32LE(data, index + 8);
                    block->offset = ReadUInt32LE(data, index + 12);
                }
                extraBlocks.push_back(block);
                break;
            }
            case 0xA0000003: { // TrackerDataBaseBlock
                auto block = std::make_shared<TrackerDataBaseBlock>();
                block->size = size;
                block->signature = sig;
                if (size >= 0x60) {
                    block->version = static_cast<int32_t>(ReadUInt32LE(data, index + 8));

                    // Machine ID: 16 bytes at offset 0x10, null-terminated, codepage-encoded
                    if (index + 32 <= raw.size()) {
                        size_t len = 0;
                        for (int i = 0; i < 16; ++i) {
                            if (data[index + 16 + i] == 0) break;
                            len++;
                        }
                        block->machineId = DecodeCp1252(data + index + 16, len);
                    }

                    // Droids: .NET order is VolumeDroid(0x20), FileDroid(0x30),
                    //         VolumeDroidBirth(0x40), FileDroidBirth(0x50)
                    size_t off = index + 0x20;
                    if (off + 16 <= raw.size()) {
                        block->volumeDroid = GuidToString(data + off);
                    }
                    off = index + 0x30;
                    if (off + 16 <= raw.size()) {
                        block->fileDroid = GuidToString(data + off);
                    }
                    off = index + 0x40;
                    if (off + 16 <= raw.size()) {
                        block->volumeDroidBirth = GuidToString(data + off);
                    }
                    off = index + 0x50;
                    if (off + 16 <= raw.size()) {
                        block->fileDroidBirth = GuidToString(data + off);
                    }

                    // MAC address: last segment of FileDroid GUID string
                    if (!block->fileDroid.empty()) {
                        // GUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
                        // Last segment is the last 12 hex chars
                        size_t lastDash = block->fileDroid.rfind('-');
                        if (lastDash != std::string::npos && lastDash + 1 < block->fileDroid.size()) {
                            std::string lastSeg = block->fileDroid.substr(lastDash + 1);
                            std::string mac;
                            for (size_t i = 0; i < lastSeg.size(); i += 2) {
                                if (i > 0) mac += ":";
                                mac += lastSeg.substr(i, 2);
                            }
                            block->macAddress = mac;
                        }
                    }

                    // Creation time from FileDroid GUID
                    if (!block->fileDroid.empty()) {
                        block->creationTime = GetDateTimeFromGuid(data + index + 0x30);
                    }
                }
                extraBlocks.push_back(block);
                break;
            }
            case 0xA000000C: { // VistaAndAboveIdListDataBlock
                auto block = std::make_shared<VistaAndAboveIdListDataBlock>();
                block->size = size;
                block->signature = sig;
                // Parse ID list
                if (size > 8) {
                    size_t listOff = index + 8;
                    uint16_t listSize = ReadUInt16LE(data, listOff);
                    listOff += 2;
                    size_t listEnd = listOff + listSize;
                    while (listOff < listEnd && listOff < raw.size()) {
                        uint16_t itemSize = ReadUInt16LE(data, listOff);
                        if (itemSize == 0) break;
                        if (listOff + itemSize > raw.size()) break;
                        std::vector<uint8_t> itemData(raw.begin() + listOff, raw.begin() + listOff + itemSize);
                        auto bag = ParseShellItem(itemData, codepage_);
                        if (bag) block->targetIDs.push_back(bag);
                        listOff += itemSize;
                    }
                }
                extraBlocks.push_back(block);
                break;
            }
            default: {
                // Unknown block, record as damaged
                auto block = std::make_shared<DamagedDataBlock>();
                block->size = size;
                block->signature = sig;
                block->originalSignature = sig;
                block->errorMessage = "Unknown extra data type";
                if (index + size <= raw.size()) {
                    block->rawBytes.assign(raw.begin() + index, raw.begin() + index + size);
                }
                extraBlocks.push_back(block);
                break;
            }
        }

        index += size;
    }
}

std::string LnkFile::LocalPath() const {
    if (linkInfo) {
        if (!linkInfo->localBasePathUnicode.empty()) {
            return linkInfo->localBasePathUnicode;
        }
        if (!linkInfo->localBasePath.empty()) {
            return linkInfo->localBasePath;
        }
    }
    return "";
}

std::string LnkFile::CommonPath() const {
    if (linkInfo) {
        if (!linkInfo->commonPathSuffixUnicode.empty()) {
            return linkInfo->commonPathSuffixUnicode;
        }
        if (!linkInfo->commonPathSuffix.empty()) {
            return linkInfo->commonPathSuffix;
        }
    }
    return "";
}

bool LnkFile::HasTrackerData() const {
    for (const auto& block : extraBlocks) {
        if (block->signature == 0xA0000003) return true;
    }
    return false;
}

std::shared_ptr<TrackerDataBaseBlock> LnkFile::GetTrackerData() const {
    for (const auto& block : extraBlocks) {
        if (block->signature == 0xA0000003) {
            return std::dynamic_pointer_cast<TrackerDataBaseBlock>(block);
        }
    }
    return nullptr;
}

} // namespace lecmd
