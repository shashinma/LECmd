#include "PropertyStore.h"
#include "LnkFile.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <unordered_map>

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

static std::string ReadUnicodeStringAt(const uint8_t* data, size_t& offset, size_t maxChars = 512) {
    std::string result;
    for (size_t i = 0; i < maxChars; ++i) {
        uint16_t ch = ReadUInt16LE(data, offset);
        offset += 2;
        if (ch == 0) break;
        if (ch < 0x80) {
            result += static_cast<char>(ch);
        } else if (ch < 0x800) {
            result += static_cast<char>(0xC0 | (ch >> 6));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (ch >> 12));
            result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }
    return result;
}

static std::string BytesToHex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        if (i > 0) oss << "-";
        oss << std::setw(2) << (int)data[i];
    }
    return oss.str();
}

static std::time_t FileTimeToTimeT(uint64_t filetime) {
    if (filetime == 0) return 0;
    const uint64_t EPOCH_DIFF = 116444736000000000ULL;
    if (filetime < EPOCH_DIFF) return 0;
    return static_cast<std::time_t>((filetime - EPOCH_DIFF) / 10000000);
}

static std::string FormatFileTime(uint64_t ft) {
    std::time_t t = FileTimeToTimeT(ft);
    if (t == 0) return "";
    struct tm tm = {};
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

static std::string ProcessNamedValue(const uint8_t* data, size_t& idx, uint16_t namedType, size_t maxLen);
static std::string ProcessNumericValue(const uint8_t* data, size_t& idx, uint16_t numericType, size_t maxLen);

PropertyStore::PropertyStore(const std::vector<uint8_t>& rawBytes) {
    size_t shellPropertyIndex = 0;
    while (shellPropertyIndex < rawBytes.size()) {
        if (shellPropertyIndex + 4 > rawBytes.size()) break;
        int32_t serializedSize = static_cast<int32_t>(ReadUInt32LE(rawBytes.data(), shellPropertyIndex));
        if (serializedSize == 0 || static_cast<uint32_t>(serializedSize) >= rawBytes.size()) break;

        std::vector<uint8_t> sheetListBytes(rawBytes.begin() + shellPropertyIndex,
                                             rawBytes.begin() + shellPropertyIndex + serializedSize);

        PropertySheet sheet;
        size_t sheetIndex = 0;
        int32_t sheetSize = static_cast<int32_t>(ReadUInt32LE(sheetListBytes.data(), sheetIndex));
        sheetIndex = 4;

        // Version: "1SPS" = 0x31 0x53 0x50 0x53
        if (sheetIndex + 4 > sheetListBytes.size()) break;
        if (sheetListBytes[sheetIndex] != 0x31 || sheetListBytes[sheetIndex+1] != 0x53 ||
            sheetListBytes[sheetIndex+2] != 0x50 || sheetListBytes[sheetIndex+3] != 0x53) {
            break; // version mismatch
        }
        sheetIndex += 4;

        // GUID
        if (sheetIndex + 16 > sheetListBytes.size()) break;
        sheet.guid = GuidToString(sheetListBytes.data() + sheetIndex);
        sheetIndex += 16;

        bool isNamed = (sheet.guid == "d5cdd505-2e9c-101b-9397-08002b2cf9ae");

        if (isNamed) {
            // Named properties
            while (sheetIndex < sheetListBytes.size()) {
                if (sheetIndex + 4 > sheetListBytes.size()) break;
                int32_t valueSize = static_cast<int32_t>(ReadUInt32LE(sheetListBytes.data(), sheetIndex));
                if (valueSize == 0) break;
                sheetIndex += 4;

                if (sheetIndex + 4 > sheetListBytes.size()) break;
                int32_t nameSize = static_cast<int32_t>(ReadUInt32LE(sheetListBytes.data(), sheetIndex));
                sheetIndex += 4;

                if (sheetIndex >= sheetListBytes.size()) break;
                sheetIndex += 1; // reserved

                std::string propertyName;
                if (nameSize > 2 && sheetIndex + nameSize <= sheetListBytes.size()) {
                    for (size_t i = 0; i < (nameSize - 2) / 2; ++i) {
                        uint16_t ch = ReadUInt16LE(sheetListBytes.data(), sheetIndex);
                        sheetIndex += 2;
                        if (ch == 0) break;
                        if (ch < 0x80) propertyName += static_cast<char>(ch);
                        else if (ch < 0x800) {
                            propertyName += static_cast<char>(0xC0 | (ch >> 6));
                            propertyName += static_cast<char>(0x80 | (ch & 0x3F));
                        } else {
                            propertyName += static_cast<char>(0xE0 | (ch >> 12));
                            propertyName += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                            propertyName += static_cast<char>(0x80 | (ch & 0x3F));
                        }
                    }
                }

                if (sheetIndex + 4 > sheetListBytes.size()) break;
                uint16_t namedType = ReadUInt16LE(sheetListBytes.data(), sheetIndex);
                sheetIndex += 2; // type
                sheetIndex += 2; // padding

                std::string value = ProcessNamedValue(sheetListBytes.data(), sheetIndex, namedType, sheetListBytes.size() - sheetIndex);
                std::string desc = GetPropertyDescription(sheet.guid, 0);
                sheet.properties.emplace_back(propertyName, desc, value);
            }
        } else {
            // Numeric properties
            while (sheetIndex < sheetListBytes.size()) {
                if (sheetIndex + 4 > sheetListBytes.size()) break;
                int32_t sheetSize2 = static_cast<int32_t>(ReadUInt32LE(sheetListBytes.data(), sheetIndex));
                if (sheetSize2 == 0 || static_cast<uint32_t>(sheetSize2) >= sheetListBytes.size()) break;
                sheetIndex += 4;

                if (sheetIndex + 4 > sheetListBytes.size()) break;
                int32_t propertyId = static_cast<int32_t>(ReadUInt32LE(sheetListBytes.data(), sheetIndex));
                sheetIndex += 4;

                if (sheetIndex >= sheetListBytes.size()) break;
                sheetIndex += 1; // reserved

                if (sheetIndex + 4 > sheetListBytes.size()) break;
                uint16_t numericType = ReadUInt16LE(sheetListBytes.data(), sheetIndex);
                sheetIndex += 2; // type
                sheetIndex += 2; // padding

                std::string value = ProcessNumericValue(sheetListBytes.data(), sheetIndex, numericType, sheetListBytes.size() - sheetIndex);
                std::string desc = GetPropertyDescription(sheet.guid, propertyId);
                sheet.properties.emplace_back(std::to_string(propertyId), desc, value);
            }
        }

        sheets.push_back(std::move(sheet));
        shellPropertyIndex += serializedSize;
    }
}


// VT type constants
static constexpr uint16_t VT_EMPTY      = 0x0000;
static constexpr uint16_t VT_NULL       = 0x0001;
static constexpr uint16_t VT_I2         = 0x0002;
static constexpr uint16_t VT_I4         = 0x0003;
static constexpr uint16_t VT_R4         = 0x0004;
static constexpr uint16_t VT_R8         = 0x0005;
static constexpr uint16_t VT_BSTR       = 0x0008;
static constexpr uint16_t VT_BOOL       = 0x000B;
static constexpr uint16_t VT_I1         = 0x0010;
static constexpr uint16_t VT_UI1        = 0x0011;
static constexpr uint16_t VT_UI2        = 0x0012;
static constexpr uint16_t VT_UI4        = 0x0013;
static constexpr uint16_t VT_I8         = 0x0014;
static constexpr uint16_t VT_UI8        = 0x0015;
static constexpr uint16_t VT_LPSTR      = 0x001E;
static constexpr uint16_t VT_LPWSTR     = 0x001F;
static constexpr uint16_t VT_FILETIME   = 0x0040;
static constexpr uint16_t VT_BLOB       = 0x0041;
static constexpr uint16_t VT_STREAM     = 0x0042;
static constexpr uint16_t VT_CLSID      = 0x0048;
static constexpr uint16_t VT_VECTOR     = 0x1000;

static std::string ProcessValue(const uint8_t* data, size_t& idx, uint16_t type, size_t maxLen, bool named);

static std::string ProcessNamedValue(const uint8_t* data, size_t& idx, uint16_t namedType, size_t maxLen) {
    return ProcessValue(data, idx, namedType, maxLen, true);
}

static std::string ProcessNumericValue(const uint8_t* data, size_t& idx, uint16_t numericType, size_t maxLen) {
    return ProcessValue(data, idx, numericType, maxLen, false);
}

static std::string ProcessValue(const uint8_t* data, size_t& idx, uint16_t type, size_t maxLen, bool named) {
    uint16_t baseType = type & ~VT_VECTOR;
    bool isVector = (type & VT_VECTOR) != 0;

    if (!isVector) {
        switch (baseType) {
            case VT_I2:
                if (maxLen >= 2) { idx += 2; return std::to_string(static_cast<int16_t>(ReadUInt16LE(data, idx - 2))); }
                return "";
            case VT_I4:
                if (maxLen >= 4) { idx += 4; return std::to_string(static_cast<int32_t>(ReadUInt32LE(data, idx - 4))); }
                return "";
            case VT_I8:
                if (maxLen >= 8) { idx += 8; return std::to_string(static_cast<int64_t>(ReadUInt64LE(data, idx - 8))); }
                return "";
            case VT_UI1:
                if (maxLen >= 1) { idx += 1; return std::to_string(data[idx - 1]); }
                return "";
            case VT_UI2:
                if (maxLen >= 2) { idx += 2; return std::to_string(ReadUInt16LE(data, idx - 2)); }
                return "";
            case VT_UI4:
                if (maxLen >= 4) { idx += 4; return std::to_string(ReadUInt32LE(data, idx - 4)); }
                return "";
            case VT_UI8:
                if (maxLen >= 8) { idx += 8; return std::to_string(ReadUInt64LE(data, idx - 8)); }
                return "";
            case VT_R4:
                if (maxLen >= 4) { idx += 4; return std::to_string(ReadUInt32LE(data, idx - 4)); } // raw uint
                return "";
            case VT_R8:
                if (maxLen >= 8) { idx += 8; return std::to_string(ReadUInt64LE(data, idx - 8)); } // raw uint64
                return "";
            case VT_BOOL:
                if (maxLen >= 2) { idx += 2; return ReadUInt16LE(data, idx - 2) != 0 ? "True" : "False"; }
                return "";
            case VT_LPSTR: {
                if (maxLen < 4) return "";
                uint32_t len = ReadUInt32LE(data, idx);
                idx += 4;
                if (len == 0) return "";
                if (len > maxLen - 4) return "";
                std::string s(reinterpret_cast<const char*>(data + idx), len);
                idx += len;
                return s;
            }
            case VT_LPWSTR: {
                if (maxLen < 4) return "";
                uint32_t charCount = ReadUInt32LE(data, idx);
                idx += 4;
                if (charCount == 0) return "";
                std::string result;
                for (uint32_t i = 0; i < charCount && maxLen >= 4 + (i+1)*2; ++i) {
                    uint16_t ch = ReadUInt16LE(data, idx);
                    idx += 2;
                    if (ch == 0) break;
                    if (ch < 0x80) result += static_cast<char>(ch);
                    else if (ch < 0x800) {
                        result += static_cast<char>(0xC0 | (ch >> 6));
                        result += static_cast<char>(0x80 | (ch & 0x3F));
                    } else {
                        result += static_cast<char>(0xE0 | (ch >> 12));
                        result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (ch & 0x3F));
                    }
                }
                return result;
            }
            case VT_FILETIME: {
                if (maxLen < 8) return "";
                uint64_t ft = ReadUInt64LE(data, idx);
                idx += 8;
                return FormatFileTime(ft);
            }
            case VT_BLOB: {
                if (maxLen < 4) return "";
                uint32_t blobSize = ReadUInt32LE(data, idx);
                idx += 4;
                if (blobSize > maxLen - 4) return "";
                std::string result = BytesToHex(data + idx, blobSize);
                idx += blobSize;
                return result;
            }
            case VT_BSTR: {
                if (maxLen < 4) return "";
                uint32_t strLen = ReadUInt32LE(data, idx);
                idx += 4;
                if (strLen > maxLen - 4) return "";
                std::string s(reinterpret_cast<const char*>(data + idx), strLen);
                idx += strLen;
                return s;
            }
            case VT_CLSID: {
                if (maxLen < 16) return "";
                std::string g = GuidToString(data + idx);
                idx += 16;
                return g;
            }
            case VT_I1:
                if (maxLen >= 1) { idx += 1; return std::to_string(static_cast<int8_t>(data[idx - 1])); }
                return "";
            case VT_STREAM: {
                if (maxLen < 4) return "";
                uint32_t streamSize = ReadUInt32LE(data, idx);
                idx += 4;
                if (streamSize > maxLen - 4) return "";
                std::string s(reinterpret_cast<const char*>(data + idx), streamSize);
                idx += streamSize;
                return s;
            }
            case VT_EMPTY:
            case VT_NULL:
            default:
                return "";
        }
    } else {
        // VT_VECTOR
        if (maxLen < 4) return "";
        uint32_t count = ReadUInt32LE(data, idx);
        idx += 4;
        std::string result;
        for (uint32_t i = 0; i < count; ++i) {
            if (!result.empty()) result += ", ";
            size_t before = idx;
            std::string item = ProcessValue(data, idx, baseType, maxLen - (idx - before), named);
            result += item;
            if (idx <= before) break; // prevent infinite loop
        }
        return result;
    }
}

static const std::unordered_map<std::string, std::vector<std::pair<int, std::string>>> g_propertyStoreInfo = {
    {"46588ae2-4cbc-4338-bbfc-139326986dce", {
        {4, "SID"},
    }},
    {"dabd30ed-0043-4789-a7f8-d013a4736622", {
        {100, "Item Folder Path Display Narrow"},
    }},
    {"28636aa6-953d-11d2-b5d6-00c04fd918d0", {
        {0, "Find Data"},
        {1, "Network Resource"},
        {2, "Description ID"},
        {3, "Which Folder"},
        {4, "Network Location"},
        {5, "Computer Name"},
        {6, "Namespace CLSID"},
        {8, "Item Path Display Narrow"},
        {9, "Perceived Type"},
        {10, "Computer Simple Name"},
        {11, "Item Type"},
        {12, "File Count"},
        {14, "Total File Size"},
        {22, "Max Stack Count"},
        {23, "List Description"},
        {24, "Parsing Name"},
        {25, "SFGAO Flags"},
        {26, "Order"},
        {27, "Computer Description"},
        {29, "Contained Items"},
        {30, "Parsing Path"},
        {31, "Network Provider"},
        {32, "Delegate ID List"},
        {33, "Is SendTo Target"},
        {34, "Hide On Desktop"},
        {35, "Network Places Default Name"},
        {36, "Storage System Type"},
        {37, "Item SubType"},
    }},
    {"9f4c2855-9f79-4b39-a8d0-e1d42de1d5f3", {
        {2, "App User Model Relaunch Command"},
        {3, "App User Model Relaunch Icon Resource"},
        {4, "App User Model Relaunch Display Name Resource"},
        {5, "App User Model ID"},
        {6, "App User Model Is DestList Separator"},
        {7, "App User Model Is DestList Link"},
        {8, "App User Model Exclude From Show In New Install"},
        {9, "App User Model Prevent Pinning"},
        {10, "App User Model Best Shortcut"},
        {11, "App User Model Is Dual Mode"},
        {12, "App User Model Start Pin Option"},
        {13, "App User Model Relevance"},
        {14, "App User Model Host Environment"},
        {15, "App User Model Package Install Path"},
        {16, "App User Model Record State"},
        {17, "App User Model Package Family Name"},
        {18, "App User Model Installed By"},
        {19, "App User Model Parent ID"},
        {20, "App User Model Activation Context"},
        {21, "App User Model Package Full Name"},
        {22, "App User Model Package Relative Application ID"},
        {23, "App User Model Excluded From Launcher"},
        {24, "App User Model AppCompat ID"},
        {25, "App User Model Run Flags"},
        {26, "App User Model Toast Activator CLSID"},
        {27, "App User Model DestList Provided Title"},
        {28, "App User Model DestList Provided Description"},
        {29, "App User Model DestList Logo Uri"},
        {30, "App User Model DestList Provided Group Name"},
    }},
    {"446d16b1-8dad-4870-a748-402ea43d788c", {
        {100, "Thumbnail Cache Id"},
        {104, "Volume Id"},
        {105, "Tooltip Thumbnail Stream"},
    }},
    {"fb8d2d7b-90d1-4e34-bf60-6eac09922bbf", {
        {2, "WinX Hash"},
    }},
    {"f29f85e0-4ff9-1068-ab91-08002b27b3d9", {
        {3, "Subject"},
        {4, "Author"},
        {5, "Keywords"},
        {6, "Comment"},
        {7, "Document Template"},
        {8, "Document Last Author"},
        {9, "Document Revision Number"},
        {10, "Document Total Editing Time"},
        {11, "Document Date Printed"},
        {12, "Document Date Created"},
        {13, "Document Date Saved"},
        {14, "Document Page Count"},
        {15, "Document Word Count"},
        {16, "Document Character Count"},
        {17, "Thumbnail"},
        {18, "Application Name"},
        {19, "Document Security"},
        {24, "High Keywords"},
        {25, "Low Keywords"},
        {26, "Medium Keywords"},
        {27, "Thumbnail Stream"},
    }},
    {"841e4f90-ff59-4d16-8947-e81bbffab36d", {
        {2, "Publisher Display Name"},
        {3, "Software Registered Owner"},
        {4, "Software Registered Company"},
        {5, "Software AppId"},
        {6, "Software Support Url"},
        {7, "Software Support Telephone"},
        {8, "Software Help Link"},
        {9, "Software Install Location"},
        {10, "Software Install Source"},
        {11, "Software Date Installed"},
        {12, "Software Support Contact Name"},
        {13, "Software ReadMe Url"},
        {14, "Software Update Info Url"},
        {15, "Software Times Used"},
        {16, "Software Date Last Used"},
        {17, "Software Tasks File Url"},
        {18, "Software Parent Name"},
        {19, "Software Product ID"},
        {20, "Software Comments"},
        {997, "Software Null Preview Total Size"},
        {998, "Software Null Preview Subtitle"},
        {999, "Software Null Preview Title"},
    }},
    {"86d40b4d-9069-443c-819a-2a54090dccec", {
        {2, "Tile Small Image Location"},
        {4, "Tile Background Color"},
        {5, "Tile Foreground Color"},
        {11, "Tile Display Name"},
        {12, "Tile Image Location"},
        {13, "Tile Wide 310x150 Logo Path"},
        {14, "Tile Unknown Flags"},
        {15, "Tile Badge Logo Path"},
        {16, "Tile Suite Display Name"},
        {17, "Tile Suite Sor tName"},
        {18, "Tile Display Name Language"},
        {19, "Tile Square 310x310 Logo Path"},
        {20, "Tile Square 70x70 Logo Path"},
        {21, "Tile Fence Post"},
        {22, "Tile Install Progress"},
        {23, "Tile Encoded Target Path"},
    }},
    {"b725f130-47ef-101a-a5f1-02608c9eebac", {
        {2, "Item Folder Name Display"},
        {3, "Search ClassID"},
        {4, "Item Type Text"},
        {8, "File Index"},
        {9, "Search Last Change USN"},
        {10, "Item Name Display"},
        {12, "Size"},
        {13, "File Attributes"},
        {14, "Date Modified"},
        {15, "Date Created"},
        {16, "Date Accessed"},
        {18, "File Allocation Size"},
        {19, "Search Contents"},
        {20, "Search ShortName"},
        {21, "File FRN"},
        {22, "Search Scope"},
        {23, "Item Name Sort Override"},
        {24, "Item Name Display Without Extension"},
        {25, "Folder Name Display"},
    }},
    {"e3e0584c-b788-4a5a-bb20-7f5a44c9acdd", {
        {2, "Message Bcc Address"},
        {3, "Message Bcc Name"},
        {4, "Message Cc Address"},
        {5, "Message Cc Name"},
        {6, "Item Folder Path Display"},
        {7, "Item Path Display"},
        {9, "Communication Account Name"},
        {10, "Is Read"},
        {11, "Importance"},
        {12, "Flag Status"},
        {13, "Message From Address"},
        {14, "Message From Name"},
        {15, "Message Store"},
        {16, "Message To Address"},
        {17, "Message To Name"},
        {18, "Contact Web Page"},
        {19, "Message Date Sent"},
        {20, "Message Date Received"},
        {21, "Message Attachment Names"},
    }},
    {"00000000-0000-0000-0000-000000000000", {
        {0, "Null"},
    }},
    {"00bc20a3-bd48-4085-872c-a88d77f5097e", {
        {105, "Music Composer Sort Override"},
    }},
    {"00f58a38-c54b-4c40-8696-97235980eae1", {
        {100, "Calendar Resources"},
    }},
    {"00f63dd8-22bd-4a5d-ba34-5cb0b9bdcb03", {
        {101, "Contact Job Info1 Yomi Company Name"},
        {102, "Contact Job Info1 Company Name"},
        {103, "Contact Job Info1 Title"},
        {104, "Contact Job Info1 Office Location"},
        {105, "Contact Job Info1 Manager"},
        {106, "Contact Job Info1 Department"},
        {107, "Contact Job Info2 Yomi Company Name"},
        {108, "Contact Job Info2 Company Name"},
        {109, "Contact Job Info2 Title"},
        {110, "Contact Job Info2 Office Location"},
        {112, "Contact Job Info2 Manager"},
        {113, "Contact Job Info2 Department"},
        {114, "Contact Job Info3 Yomi Company Name"},
        {115, "Contact Job Info3 Company Name"},
        {116, "Contact Job Info3 Title"},
        {117, "Contact Job Info3 Office Location"},
        {118, "Contact Job Info3 Manager"},
        {119, "Contact Job Info3 Department"},
        {120, "Contact Job Info1 Company Address"},
        {121, "Contact Job Info2 Company Address"},
        {123, "Contact Job Info3 Company Address"},
        {124, "Contact Webpage 2"},
        {125, "Contact Webpage 3"},
    }},
    {"026e516e-b814-414b-83cd-856d6fef4822", {
        {3, "Devices Interface Enabled"},
        {4, "Devices Interface Class Guid"},
        {6, "Devices Restricted Interface"},
    }},
    {"029c0252-5b86-46c7-aca0-2769ffc8e3d4", {
    }},
    {"02b0f689-a914-4e45-821d-1dda452ed2c4", {
    }},
    {"03089873-8ee8-4191-bd60-d31f72b7900b", {
    }},
    {"0337ecec-39fb-4581-a0bd-4c4cc51e9914", {
    }},
    {"048658ad-2db8-41a4-bbb6-ac1ef1207eb1", {
    }},
    {"05e932b1-7ca2-491f-bd69-99b4cb266cbb", {
    }},
    {"06704b0c-e830-4c81-9178-91e4e95a80a0", {
    }},
    {"084d8a0a-e6d5-40de-bf1f-c8820e7c877c", {
    }},
    {"08a65aa1-f4c9-43dd-9ddf-a33d8e7ead85", {
    }},
    {"08c7cc5f-60f2-4494-ad75-55e3e0b5add0", {
    }},
    {"08f6d7c2-e3f2-44fc-af1e-5aa5c81a2d3e", {
    }},
    {"09329b74-40a3-4c68-bf07-af9a572f607c", {
    }},
    {"0933f3f5-4786-4f46-a8e8-d64dd37fa521", {
    }},
    {"09429607-582d-437f-84c3-de93a2b24c3c", {
    }},
    {"09736039-456b-4219-ba3e-ec573b58cf97", {
    }},
    {"09edd5b6-b301-43c5-9990-d00302effd46", {
    }},
    {"0a7b84ef-0c27-463f-84ef-06c5070001be", {
    }},
    {"0abe4d16-9384-426b-b41a-eac3c8e0f147", {
    }},
    {"0adef160-db3f-4308-9a21-06237b16fa2a", {
    }},
    {"0b48f35a-be6e-4f17-b108-3c4073d1669a", {
    }},
    {"0b63e343-9ccc-11d0-bcdb-00805fccce04", {
    }},
    {"0b63e350-9ccc-11d0-bcdb-00805fccce04", {
    }},
    {"0b8bb018-2725-4b44-92ba-7933aeb2dde7", {
    }},
    {"0ba7d6c3-568d-4159-ab91-781a91fb71e5", {
    }},
    {"0bba1ede-7566-4f47-90ec-25fc567ced2a", {
    }},
    {"0be1c8e7-1981-4676-ae14-fdd78f05a6e7", {
    }},
    {"0be3fd71-3f87-40e0-aead-0294cf674635", {
    }},
    {"0c73b141-39d6-4653-a683-cab291eaf95b", {
    }},
    {"0c840a88-b043-466d-9766-d4b26da3fa77", {
    }},
    {"0cb2bf5a-9ee7-4a86-8222-f01e07fdadaf", {
    }},
    {"0cef7d53-fa64-11d1-a203-0000f81fedee", {
    }},
    {"0cf8fb02-1837-42f1-a697-a7017aa289b9", {
    }},
    {"0da41cfa-d224-4a18-ae2f-596158db4b3a", {
    }},
    {"0ded77b3-c614-456c-ae5b-285b38d7b01b", {
    }},
    {"0f55cde2-4f49-450d-92c1-dcd16301b1b7", {
    }},
    {"10984e0a-f9f2-4321-b7ef-baf195af4319", {
    }},
    {"10b24595-41a2-4e20-93c2-5761c1395f32", {
    }},
    {"10dabe05-32aa-4c29-bf1a-63e2d220587f", {
    }},
    {"1173f62a-2a55-4f62-aed6-8c7112e0f7a3", {
    }},
    {"11d6336b-38c4-4ec9-84d6-eb38d0b150af", {
    }},
    {"125491f4-818f-46b2-91b5-d537753617b2", {
    }},
    {"12ea418f-d8cd-4cdf-9b23-457eaac7ff0d", {
    }},
    {"12fa14f5-c6fe-4545-bce2-1ed6cb6b8422", {
    }},
    {"13673f42-a3d6-49f6-b4da-ae46e0c5237c", {
    }},
    {"13eb7ffc-ec89-4346-b19d-ccc6f1784223", {
    }},
    {"14977844-6b49-4aad-a714-a4513bf60460", {
    }},
    {"149c0b69-2c2d-48fc-808f-d318d78c4636", {
    }},
    {"14b81da1-0135-4d31-96d9-6cbfc9671a99", {
    }},
    {"1506935d-e3e7-450f-8637-82233ebe5f6e", {
    }},
    {"16473c91-d017-4ed9-ba4d-b6baa55dbcf8", {
    }},
    {"16cbb924-6500-473b-a5be-f1599bcbe413", {
    }},
    {"16e634ee-2bff-497b-bd8a-4341ad39eeb9", {
    }},
    {"16ea4042-d6f4-4bca-8349-7c78d30fb333", {
    }},
    {"176dc63c-2688-4e89-8143-a347800f25e9", {
    }},
    {"1804d1fb-9fa4-441d-a536-76468ac43307", {
    }},
    {"182c1ea6-7c1c-4083-ab4b-ac6c9f4ed128", {
    }},
    {"188c1f91-3c40-4132-9ec5-d8b03b72a8a2", {
    }},
    {"18bbd425-ecfd-46ef-b612-7b4a6034eda0", {
    }},
    {"19b51fa6-1f92-4a5c-ab48-7df0abd67444", {
    }},
    {"1a701bf6-478c-4361-83ab-3701bb053c58", {
    }},
    {"1a9ba605-8e7c-4d11-ad7d-a50ada18ba1b", {
    }},
    {"1b5439e7-eba1-4af8-bdd7-7af1d4549493", {
    }},
    {"1b97738a-fdfc-462f-9d93-1957e08be90c", {
    }},
    {"30c8eef4-a832-41e2-ab32-e3c3ca28fd29", {
    }},
    {"3143bf7c-80a8-4854-8880-e2e40189bdd0", {
    }},
    {"315b9c8d-80a9-4ef9-ae16-8e746da51d70", {
    }},
    {"318a6b45-087f-4dc2-b8cc-05359551fc9e", {
    }},
    {"31b37743-7c5e-4005-93e6-e953f92b82e9", {
    }},
    {"328d8b21-7729-4bfc-954c-902b329d56b0", {
    }},
    {"32bcb03c-7f34-4e3f-bbb2-ebe63629f5e4", {
    }},
    {"33dcf22b-28d5-464c-8035-1ee9efd25278", {
    }},
    {"341796f1-1df9-4b1c-a564-91bdefa43877", {
    }},
    {"346c8bd1-2e6a-4c45-89a4-61b78e8e700f", {
    }},
    {"35dbe6fe-44c3-4400-aaae-d2c799c407e8", {
    }},
    {"3602c812-0f3b-45f0-85ad-603468d69423", {
    }},
    {"3633de59-6825-4381-a49b-9f6ba13a1471", {
    }},
    {"364028da-d895-41fe-a584-302b1bb70a76", {
    }},
    {"364b6fa9-37ab-482a-be2b-ae02f60d4318", {
    }},
    {"37ebd11f-7e72-4ebc-9d4c-c790f8c277c2", {
    }},
    {"38965063-edc8-4268-8491-b7723172cf29", {
    }},
    {"38d43380-d418-4830-84d5-46935a81c5c6", {
    }},
    {"39a7f922-477c-48de-8bc8-b28441e342e3", {
    }},
    {"39b77f4f-a104-4863-b395-2db2ad8f7bc1", {
    }},
    {"3a372292-7fca-49a7-99d5-e47bb2d4e7ab", {
    }},
    {"3b2ce006-5e61-4fde-bab8-9b8aac9b26df", {
    }},
    {"3c8cee58-d4f0-4cf9-b756-4e5d24447bcd", {
    }},
    {"3d658d4d-bc38-464a-b555-418d554a8df8", {
    }},
    {"3d75e4f5-a391-4952-81f7-c7072fe53025", {
    }},
    {"3f08e66f-2f44-4bb9-a682-ac35d2562322", {
    }},
    {"3f5d9b45-5e9f-4d5c-8a5e-403181bf177b", {
    }},
    {"3f8472b5-e0af-4db2-8071-c53fe76ae7ce", {
    }},
    {"402b5934-ec5a-48c3-93e6-85e86a2d934e", {
    }},
    {"41cf5ae0-f75a-4806-bd87-59c7d9248eb9", {
    }},
    {"425d69e5-48ad-4900-8d80-6eb6b8d0ac86", {
    }},
    {"428040ac-a177-4c8a-9760-f6f761227f9a", {
    }},
    {"42864dfd-9da4-4f77-bded-4aad7b256735", {
    }},
    {"4340a6c5-93fa-4706-972c-7b648008a5a7", {
    }},
    {"436f2667-14e2-4feb-b30a-146c53b5b674", {
    }},
    {"43f8d7b7-a444-4f87-9383-52271c9b915c", {
    }},
    {"446f787f-10c4-41cb-a6c4-4d0343551597", {
    }},
    {"4530d076-b598-4a81-8813-9b11286ef6ea", {
    }},
    {"4596208c-32fa-41d2-9695-af0cb9e8dcfe", {
    }},
    {"45eae747-8e2a-40ae-8cbf-ca52aba6152a", {
    }},
    {"4679c1b5-844d-4590-baf5-f322231f1b81", {
    }},
    {"467ee575-1f25-4557-ad4e-b8b58b0d9c15", {
    }},
    {"4684fe97-8765-4842-9c13-f006447b178c", {
    }},
    {"46ac629d-75ea-4515-867f-6dc4321c5844", {
    }},
    {"46b4e8de-cdb2-440d-885c-1658eb65b914", {
    }},
    {"47166b16-364f-4aa0-9f31-e2ab3df449c3", {
    }},
    {"4776cafa-bce4-4cb1-a23e-265e76d8eb11", {
    }},
    {"47a96261-cb4c-4807-8ad3-40b9d9dbc6bc", {
    }},
    {"48fd6ec8-8a12-4cdf-a03e-4ec5a511edde", {
    }},
    {"49237325-a95a-4f67-b211-816b2d45d2e0", {
    }},
    {"49691c90-7e17-101a-a91c-08002b2ecda9", {
    }},
    {"49753869-849c-4323-a41f-26d73f28b53b", {
    }},
    {"49cd1f76-5626-4b17-a4e8-18b4aa1a2213", {
    }},
    {"49d1091f-082e-493f-b23f-d2308aa9668c", {
    }},
    {"49eb6558-c09c-46dc-8668-1f848c290d0b", {
    }},
    {"4ac903f8-e780-4e4b-b7b8-4d00a99804fc", {
    }},
    {"4b486401-5468-4381-9b5a-42df4cb49f53", {
    }},
    {"4bd13b3d-e68b-44ec-89ee-7611789d4070", {
    }},
    {"4c6bf15c-4c03-4aac-91f5-64c0f852bcf4", {
    }},
    {"4d1ebee8-0803-4774-9842-b77db50265e9", {
    }},
    {"4e9cfc01-5d36-406a-83cd-4e7423923604", {
    }},
    {"4f289a46-2bbb-4ae8-9eda-e5e034707a71", {
    }},
    {"4fffe4d0-914f-4ac4-8d6f-c9c61de169b1", {
    }},
    {"502cfeab-47eb-459c-b960-e6d8728f7701", {
    }},
    {"5068bcdf-d697-4d85-8c53-1f1cdab01763", {
    }},
    {"508161fa-313b-43d5-83a1-c1accf68622c", {
    }},
    {"51236583-0c4a-4fe8-b81f-166aec13f510", {
    }},
    {"51ec3f47-dd50-421d-8769-334f50424b1e", {
    }},
    {"53da57cf-62c0-45c4-81de-7610bcefd7f5", {
    }},
    {"540b947e-8b40-45bc-a8a2-6a0b894cbda2", {
    }},
    {"54b3a473-59aa-445b-aecd-77541ba8b7c9", {
    }},
    {"5567bf77-2be2-4222-befa-d0c9c9cc4b6e", {
    }},
    {"55e98597-ad16-42e0-b624-21599a199838", {
    }},
    {"560c36c0-503a-11cf-baa1-00004c752a9a", {
    }},
    {"56310920-2491-4919-99ce-eadb06fafdb2", {
    }},
    {"56a3372e-ce9c-11d2-9f0e-006097c686f6", {
    }},
    {"56c90e9d-9d46-4963-886f-2e1cd9a694ef", {
    }},
    {"57086c23-86c6-478f-afb2-236188c8f47f", {
    }},
    {"5741cf9c-56fe-485b-8901-4786449e188d", {
    }},
    {"59569556-0a08-4212-95b9-fae2ad6413db", {
    }},
    {"596fd41b-af9b-4ba8-9b49-33b16f16678c", {
    }},
    {"59d49e61-840f-4aa9-a939-e2099b7f6399", {
    }},
    {"59dde9f2-5253-40ea-9a8b-479e96c6249a", {
    }},
    {"5ab5c75f-15e1-4d65-924a-04754567243c", {
    }},
    {"5bf396d4-5eb2-466f-bde9-2fb3f2361d6e", {
    }},
    {"5cbf2787-48cf-4208-b90e-ee5e5d420294", {
    }},
    {"5cda5fc8-33ee-4ff3-9094-ae7bd8868c4d", {
    }},
    {"5cde9f0e-1de4-4453-96a9-56e8832efa3d", {
    }},
    {"5d76b67f-9b3d-44bb-b6ae-25da4f638a67", {
    }},
    {"5da84765-e3ff-4278-86b0-a27967fbdd03", {
    }},
    {"5dc2253f-5e11-4adf-9cfe-910dd01e3e70", {
    }},
    {"5f5aff6a-37e5-4780-97ea-80c7565cf535", {
    }},
    {"5fbd34cd-561a-412e-ba98-478a6b0fef1d", {
    }},
    {"61478c08-b600-4a84-bbe4-e99c45f0a072", {
    }},
    {"61872cf7-6b5e-4b4b-ac2d-59da84459248", {
    }},
    {"62d2d9ab-8b64-498d-b865-402d4796f865", {
    }},
    {"6336b95e-c7a7-426d-86fd-7ae3d39c84b4", {
    }},
    {"635e9051-50a5-4ba2-b9db-4ed056c77296", {
    }},
    {"63c25b20-96be-488f-8788-c09c407ad812", {
    }},
    {"641064ba-9329-47e6-8f36-5fa81aa461a0", {
    }},
    {"6444048f-4c8b-11d1-8b70-080036b11a03", {
    }},
    {"64440490-4c8b-11d1-8b70-080036b11a03", {
    }},
    {"64440491-4c8b-11d1-8b70-080036b11a03", {
    }},
    {"64440492-4c8b-11d1-8b70-080036b11a03", {
    }},
    {"644d37b4-e1b3-4bad-b099-7e7c04966aca", {
    }},
    {"656a3bb3-ecc0-43fd-8477-4ae0404a96cd", {
    }},
    {"65a98875-3c80-40ab-abbc-efdaf77dbee2", {
    }},
    {"660e04d6-81ab-4977-a09f-82313113ab26", {
    }},
    {"6614ef48-4efe-4424-9eda-c79f404edf3e", {
    }},
    {"668cdfa5-7a1b-4323-ae4b-e527393a1d81", {
    }},
    {"67df94de-0ca7-4d6f-b792-053a3e4f03cf", {
    }},
    {"6845cc72-1b71-48c3-af86-b09171a19b14", {
    }},
    {"68dd6094-7216-40f1-a029-43fe7127043f", {
    }},
    {"6a15e5a0-0a1e-4cd7-bb8c-d2f1b0c929bc", {
    }},
    {"6af55d45-38db-4495-acb0-d4728a3b8314", {
    }},
    {"6afe7437-9bcd-49c7-80fe-4a5c65fa5874", {
    }},
    {"6b223b6a-162e-4aa9-b39f-05d678fc6d77", {
    }},
    {"6b8b68f6-200b-47ea-8d25-d8050f57339f", {
    }},
    {"6b8da074-3b5c-43bc-886f-0a2cdce00b6f", {
    }},
    {"6bdd1fc6-810f-11d0-bec7-08002be2092f", {
    }},
    {"6ccd0131-c397-4744-b2d8-d2c13f457026", {
    }},
    {"6d217f6d-3f6a-4825-b470-5f03ca2fbe9b", {
    }},
    {"6d24888f-4718-4bda-afed-ea0fb4386cd8", {
    }},
    {"6d6d5d49-265d-4688-9f4e-1fdd33e7cc83", {
    }},
    {"6d748de2-8d38-4cc3-ac60-f009b057c557", {
    }},
    {"6e682923-7f7b-4f0c-a337-cfca296687bf", {
    }},
    {"6ebe6946-2321-440a-90f0-c043efd32476", {
    }},
    {"6fa20de6-d11c-4d9d-a154-64317628c12d", {
    }},
    {"702926f4-44a6-43e1-ae71-45627116893b", {
    }},
    {"7036dcfc-69ab-4316-b5ac-50de702447b0", {
    }},
    {"705ccb0f-5a0d-41ea-b2ca-2c9b5cc7db41", {
    }},
    {"705d8364-7547-468c-8c88-84860bcbed4c", {
    }},
    {"71724756-3e74-4432-9b59-e7b2f668a593", {
    }},
    {"71b377d6-e570-425f-a170-809fae73e54e", {
    }},
    {"720eb626-dbe4-4113-835c-9315e1e2ff77", {
    }},
    {"7268af55-1ce4-4f6e-a41f-b6e4ef10e4a9", {
    }},
    {"72fab781-acda-43e5-b155-b2434f85e678", {
    }},
    {"72fc5ba4-24f9-4011-9f3f-add27afad818", {
    }},
    {"730fb6dd-cf7c-426b-a03f-bd166cc9ee24", {
    }},
    {"73389854-0b42-4ea6-bc67-847d430899fd", {
    }},
    {"733cb147-8b1f-4c48-9966-192fde353c75", {
    }},
    {"738bf284-1d87-420b-92cf-5834bf6ef9ed", {
    }},
    {"744c8242-4df5-456c-ab9e-014efb9021e3", {
    }},
    {"745baf0e-e5c1-4cfb-8a1b-d031a0a52393", {
    }},
    {"74a7de49-fa11-4d3d-a006-db7e08675916", {
    }},
    {"75ee72ae-7d5f-482f-9487-f1c46ca819c1", {
    }},
    {"76c09943-7c33-49e3-9e7e-cdba872cfada", {
    }},
    {"776b6b3b-1e3d-4b0c-9a0e-8fbaf2a8492a", {
    }},
    {"78342dcb-e358-4145-ae9a-6bfe4e0f9f51", {
    }},
    {"78c34fc8-104a-4aca-9ea4-524d52996e57", {
    }},
    {"79486778-4c6f-4dde-bc53-cd594311af99", {
    }},
    {"79d94e82-4d79-45aa-821a-74858b4e4ca6", {
    }},
    {"7a55582b-bd8c-4475-b94c-b87a388a7899", {
    }},
    {"7a7d76f4-b630-4bd7-95ff-37cc51a975c9", {
    }},
    {"7abcf4f8-7c3f-4988-ac91-8d2c2e97eca5", {
    }},
    {"7b9f6399-0a3f-4b12-89bd-4adc51c918af", {
    }},
    {"7ba3535d-69aa-4525-a938-f3ec79485377", {
    }},
    {"7bd5533e-af15-44db-b8c8-bd6624e1d032", {
    }},
    {"7d122d5a-ae5e-4335-8841-d71e7ce72f53", {
    }},
    {"7d683fc9-d155-45a8-bb1f-89d19bcb792f", {
    }},
    {"7ddaaad1-ccc8-41ae-b750-b2cb8031aea2", {
    }},
    {"7fd7259d-16b4-4135-9f97-7c96ecd2fa9e", {
    }},
    {"7fe3aa27-2648-42f3-89b0-454e5cb150c3", {
    }},
    {"807b653a-9e91-43ef-8f97-11ce04ee20c5", {
    }},
    {"80d81ea6-7473-4b0c-8216-efc11a2c4c8b", {
    }},
    {"80f41eb8-afc4-4208-aa5f-cce21a627281", {
    }},
    {"813f4124-34e6-4d17-ab3e-6b1f3c2247a1", {
    }},
    {"821437d6-9eab-4765-a589-3b1cbbd22a61", {
    }},
    {"827edb4f-5b73-44a7-891d-fdffabea35ca", {
    }},
    {"83914d1a-c270-48bf-b00d-1c4e451b0150", {
    }},
    {"83a6347e-6fe4-4f40-ba9c-c4865240d1f4", {
    }},
    {"83da6326-97a6-4088-9453-a1923f573b29", {
    }},
    {"847c66de-b8d6-4af9-abc3-6f4f926bc039", {
    }},
    {"84d8f337-981d-44b3-9615-c7596dba17e3", {
    }},
    {"8589e481-6040-473d-b171-7fa89c2708ed", {
    }},
    {"8619a4b6-9f4d-4429-8c0f-b996ca59e335", {
    }},
    {"86407db8-9df7-48cd-b986-f999adc19731", {
    }},
    {"8727cfff-4868-4ec6-ad5b-81b98521d1ab", {
    }},
    {"880f70a2-6082-47ac-8aab-a739d1a300c3", {
    }},
    {"8859a284-de7e-4642-99ba-d431d044b1ec", {
    }},
    {"8943b373-388c-4395-b557-bc6dbaffafdb", {
    }},
    {"8969b275-9475-4e00-a887-ff93b8b41e44", {
    }},
    {"897b3694-fe9e-43e6-8066-260f590c0100", {
    }},
    {"8a2f99f9-3c37-465d-a8d7-69777a246d0c", {
    }},
    {"8af4961c-f526-43e5-aa81-db768219178d", {
    }},
    {"8afcc170-8a46-4b53-9eee-90bae7151e62", {
    }},
    {"8b26ea41-058f-43f6-aecc-4035681ce977", {
    }},
    {"8bf6b9f6-b4f5-482f-a2c2-44bdad2fcfa9", {
    }},
    {"8c3b93a4-baed-1a83-9a32-102ee313f6eb", {
    }},
    {"8c7ed206-3f8a-4827-b3ab-ae9e1faefc6c", {
    }},
    {"8d72aca1-0716-419a-9ac1-acb07b18dc32", {
    }},
    {"8e531030-b960-4346-ae0d-66bc9a86fb94", {
    }},
    {"8e8ecf7c-b7b8-4eb8-a63f-0ee715c96f9e", {
    }},
    {"8f167568-0aae-4322-8ed9-6055b7b0e398", {
    }},
    {"8f367200-c270-457c-b1d4-e07c5bcd90c7", {
    }},
    {"8fdc6dea-b929-412b-ba90-397a257465fe", {
    }},
    {"900a403b-097b-4b95-8ae2-071fdaeeb118", {
    }},
    {"90197ca7-fd8f-4e8c-9da3-b57e1e609295", {
    }},
    {"908696c7-8f87-44f2-80ed-a8c1c6894575", {
    }},
    {"9098f33c-9a7d-48a8-8de5-2e1227a64e91", {
    }},
    {"90e5e14e-648b-4826-b2aa-acaf790e3513", {
    }},
    {"916d17ac-8a97-48af-85b7-867a88fad542", {
    }},
    {"91eff6f3-2e27-42ca-933e-7c999fbe310b", {
    }},
    {"93112f89-c28b-492f-8a9d-4be2062cee8a", {
    }},
    {"95beb1fc-326d-4644-b396-cd3ed90e6ddf", {
    }},
    {"95c656c1-2abf-4148-9ed3-9ec602e3b7cd", {
    }},
    {"95e127b5-79cc-4e83-9c9e-8422187b3e0e", {
    }},
    {"9660c283-fc3a-4a08-a096-eed3aac46da2", {
    }},
    {"967b5af8-995a-46ed-9e11-35b3c5b9782d", {
    }},
    {"972e333e-ac7e-49f1-8adf-a70d07a9bcab", {
    }},
    {"9744311e-7951-4b2e-b6f0-ecb293cac119", {
    }},
    {"97b0ad89-df49-49cc-834e-660974fd755b", {
    }},
    {"98f920d1-51e2-4722-9069-3c4b5cff5165", {
    }},
    {"98f98354-617a-46b8-8560-5b1b64bf1f89", {
    }},
    {"995ef0b0-7eb3-4a8b-b9ce-068bb3f4af69", {
    }},
    {"9973d2b5-bfd8-438a-ba94-5349b293181a", {
    }},
    {"9a8ebb75-6458-4e82-bacb-35c0095b03bb", {
    }},
    {"9a93244d-a7ad-4ff8-9b99-45ee4cc09af6", {
    }},
    {"9a9bc088-4f6d-469e-9919-e705412040f9", {
    }},
    {"9ab84393-2a0f-4b75-bb22-7279786977cb", {
    }},
    {"9ad5badb-cea7-4470-a03d-b84e51b9949e", {
    }},
    {"9aebae7a-9644-487d-a92c-657585ed751a", {
    }},
    {"9b174b33-40ff-11d2-a27e-00c04fc30871", {
    }},
    {"9b174b34-40ff-11d2-a27e-00c04fc30871", {
    }},
    {"9b174b35-40ff-11d2-a27e-00c04fc30871", {
    }},
    {"9b34bbb9-949c-488d-9a6d-eeb47c847a2f", {
    }},
    {"9bc2c99b-ac71-4127-9d1c-2596d0d7dcb7", {
    }},
    {"9c1fcf74-2d97-41ba-b4ae-cb2e3661a6e4", {
    }},
    {"9cb0c358-9d7a-46b1-b466-dcc6f1a3d93d", {
    }},
    {"9d1d7cc5-5c39-451c-86b3-928e2d18cc47", {
    }},
    {"9d2408b6-3167-422b-82b0-f583b7a7cfe3", {
    }},
    {"9e7d118f-b314-45a0-8cfb-d654b917c9e9", {
    }},
    {"a00742a1-cd8c-4b37-95ab-70755587767a", {
    }},
    {"a015ed5d-aaea-4d58-8a86-3c586920ea0b", {
    }},
    {"a06992b3-8caf-4ed7-a547-b259e32ac9fc", {
    }},
    {"a09f084e-ad41-489f-8076-aa5be3082bca", {
    }},
    {"a0be94c5-50ba-487b-bd35-0654be8881ed", {
    }},
    {"a0e00ee1-f0c7-4d41-b8e7-26a7bd8d38b0", {
    }},
    {"a0e74609-b84d-4f49-b860-462bd9971f98", {
    }},
    {"a11c005a-ff95-4785-8617-beaf92399c3c", {
    }},
    {"a1829ea2-27eb-459e-935d-b2fad7b07762", {
    }},
    {"a19fb7a9-024b-4371-a8bf-4d29c3e4e9c9", {
    }},
    {"a26f4afc-7346-4299-be47-eb1ae613139f", {
    }},
    {"a2e541c5-4440-4ba8-867e-75cfc06828cd", {
    }},
    {"a3250282-fb6d-48d5-9a89-dbcace75cccf", {
    }},
    {"a35996ab-11cf-4935-8b61-a6761081ecdf", {
    }},
    {"a399aac7-c265-474e-b073-ffce57721716", {
    }},
    {"a3b29791-7713-4e1d-bb40-17db85f01831", {
    }},
    {"a40294ef-d2b1-40ed-9512-dd3853b431f5", {
    }},
    {"a4108708-09df-4377-9dfc-6d99986d5a67", {
    }},
    {"a45c254e-df1c-4efd-8020-67d146a850e0", {
    }},
    {"a4790b72-7113-4348-97ea-292bbc1f6770", {
    }},
    {"a4aaa5b7-1ad0-445f-811a-0f8f6e67f6b5", {
    }},
    {"a5477f61-7a82-4eca-9dde-98b69b2479b3", {
    }},
    {"a63b464f-2ace-4d83-87ae-abaf011cc6ac", {
    }},
    {"a6744477-c237-475b-a075-54f34498292a", {
    }},
    {"a6f360d2-55f9-48de-b909-620e090a647c", {
    }},
    {"a7b6f596-d678-4bc1-b05f-0203d27e8aa1", {
    }},
    {"a7fe0840-1344-46f0-8d37-52ed712a4bf9", {
    }},
    {"a82d9ee7-ca67-4312-965e-226bcea85023", {
    }},
    {"a8a74b92-361b-4e9a-b722-7c4a7330a312", {
    }},
    {"a8a7a412-1927-4a34-b1d4-45f67cc672fb", {
    }},
    {"a93eae04-6804-4f24-ac81-09b266452118", {
    }},
    {"a94688b6-7d9f-4570-a648-e3dfc0ab2b3f", {
    }},
    {"a9ea193c-c511-498a-a06b-58e2776dcc28", {
    }},
    {"aaa660f9-9865-458e-b484-01bc7fe3973e", {
    }},
    {"aabaf6c9-e0c5-4719-8585-57b103e584fe", {
    }},
    {"aaf16bac-2b55-45e6-9f6d-415eb94910df", {
    }},
    {"aaf4ee25-bd3b-4dd7-bfc4-47f77bb00f6d", {
    }},
    {"ab205e50-04b7-461c-a18c-2f233836e627", {
    }},
    {"acc9ce3d-c213-4942-8b48-6d0820f21c6d", {
    }},
    {"ad763ac7-f1ed-4039-9fb4-b7b84ef33cef", {
    }},
    {"aeac19e4-89ae-4508-b9b7-bb867abee2ed", {
    }},
    {"afc47170-14f5-498c-8f30-b0d19be449c6", {
    }},
    {"afd97640-86a3-4210-b67c-289c41aabe55", {
    }},
    {"b0b87314-fcf6-4feb-8dff-a50da6af561c", {
    }},
    {"b180ad60-ed3f-4d16-bd43-f5b4fcf325a9", {
    }},
    {"b2f9b9d6-fec4-4dd5-94d7-8957488c807b", {
    }},
    {"b33af30b-f552-4584-936c-cb93e5cda29f", {
    }},
    {"b5c84c9e-5927-46b5-a3cc-933c21b78469", {
    }},
    {"b769d0fe-bc33-421a-8ce6-45add82ec756", {
    }},
    {"b771b352-8692-42e6-ac33-cc7b062ad950", {
    }},
    {"b7b4d61c-5a64-4187-a52e-b1539f359099", {
    }},
    {"b812f15d-c2d8-4bbf-bacd-79744346113f", {
    }},
    {"b96eff7b-35ca-4a35-8607-29e3a54c46ea", {
    }},
    {"b9b4b3fc-2b51-4a42-b5d8-324146afcf25", {
    }},
    {"ba3b1da9-86ee-4b5d-a2a4-a271a429f0cf", {
    }},
    {"bb44403b-1399-4650-95eb-03c53a57c2cf", {
    }},
    {"bc4e71ce-17f9-48d5-bee9-021df0ea5409", {
    }},
    {"bccc8a3c-8cef-42e5-9b1c-c69079398bc7", {
    }},
    {"bceee283-35df-4d53-826a-f36a3eefc6be", {
    }},
    {"be1a72c6-9a1d-46b7-afe7-afaf8cef4999", {
    }},
    {"be6e176c-4534-4d2c-ace5-31dedac1606b", {
    }},
    {"bebe0920-7671-4c54-a3eb-49fddfc191ee", {
    }},
    {"bf53d1c3-49e0-4f7f-8567-5a821d8ac542", {
    }},
    {"bf79c0ab-bb74-4cee-b070-470b5ae202ea", {
    }},
    {"bfee9149-e3e2-49a7-a862-c05988145cec", {
    }},
    {"c06238b2-0bf9-4279-a723-25856715cb9d", {
    }},
    {"c0ac206a-827e-4650-95ae-77e2bb74fcc9", {
    }},
    {"c107e191-a459-44c5-9ae6-b952ad4b906d", {
    }},
    {"c2ea046e-033c-4e91-bd5b-d4942f6bbe49", {
    }},
    {"c4322503-78ca-49c6-9acc-a68e2afd7b6b", {
    }},
    {"c449d5cb-9ea4-4809-82e8-af9d59ded6d1", {
    }},
    {"c4c07f2b-8524-4e66-ae3a-a6235f103beb", {
    }},
    {"c4c4dbb2-b593-466b-bbda-d03d27d5e43a", {
    }},
    {"c5043536-932e-219e-5fb9-1c2807d7b03e", {
    }},
    {"c53e42a9-db3c-4bc7-b0f3-83a524adf0ec", {
    }},
    {"c554493c-c1f7-40c1-a76c-ef8c0614003e", {
    }},
    {"c64a866e-41ae-4c8c-b3d5-dd6dbf70c9c1", {
    }},
    {"c66d4b3c-e888-47cc-b99f-9dca3ee34dea", {
    }},
    {"c6f039e7-f6a4-4185-ae48-07938262c274", {
    }},
    {"c75faa05-96fd-49e7-9cb4-9f601082d553", {
    }},
    {"c77724d4-601f-46c5-9b89-c53f93bceb77", {
    }},
    {"c89a23d0-7d6d-4eb8-87d4-776a82d493e5", {
    }},
    {"c8d1920c-01f6-40c0-ac86-2f3a4ad00770", {
    }},
    {"c8ea94f0-a9e3-4969-a94b-9c62a95324e0", {
    }},
    {"c9944a21-a406-48fe-8225-aec7e24c211b", {
    }},
    {"c9b88dba-04db-4887-a200-cf0d3afe1146", {
    }},
    {"c9c141a9-1b4c-4f17-a9d1-f298538cadb8", {
    }},
    {"c9c34f84-2241-4401-b607-bd20ed75ae7f", {
    }},
    {"cbf38310-4a17-4310-a1eb-247f0b67593b", {
    }},
    {"cc158e89-6581-4311-9637-a8da9002f118", {
    }},
    {"cc301630-b192-4c22-b372-9f4c6d338e07", {
    }},
    {"cc6f4f24-6083-4bd4-8754-674d0de87ab8", {
    }},
    {"cd102c9c-5540-4a88-a6f6-64e4981c8cd1", {
    }},
    {"cd9ed458-08ce-418f-a70e-f912c7bb9c5c", {
    }},
    {"cdbfc167-337e-41d8-af7c-8c09205429c7", {
    }},
    {"cdedcf30-8919-44df-8f4c-4eb2ffdb8d89", {
    }},
    {"ce50c159-2fb8-41fd-be68-d3e042e274bc", {
    }},
    {"cea820b9-ce61-4885-a128-005d9087c192", {
    }},
    {"cebf9b37-26ae-466b-9fe9-c7550c4b0ce8", {
    }},
    {"cf5751fd-f4b3-443d-b31c-9a34740759ec", {
    }},
    {"cfa31b45-525d-4998-bb44-3f7d81542fa4", {
    }},
    {"cfc08d97-c6f7-4484-89dd-ebef4356fe76", {
    }},
    {"d042d2a1-927e-40b5-a503-6edbd42a517e", {
    }},
    {"d08dd4c0-3a9e-462e-8290-7b636b2576b9", {
    }},
    {"d0a04f0a-462a-48a4-bb2f-3706e88dbd7d", {
    }},
    {"d0c7f054-3f72-4725-8527-129a577cb269", {
    }},
    {"d0dab0ba-368a-4050-a882-6c010fd19a4f", {
    }},
    {"d21a7148-d32c-4624-8900-277210f79c0f", {
    }},
    {"d35f743a-eb2e-47f2-a286-844132cb1427", {
    }},
    {"d37d52c6-261c-4303-82b3-08b926ac6f12", {
    }},
    {"d4729704-8ef1-43ef-9024-2bd381187fd5", {
    }},
    {"d4bf61b3-442e-4ada-882d-fa7b70c832d9", {
    }},
    {"d4d0aa16-9948-41a4-aa85-d97ff9646993", {
    }},
    {"d55bae5a-3892-417a-a649-c6ac5aaaeab3", {
    }},
    {"d5cdd502-2e9c-101b-9397-08002b2cf9ae", {
    }},
    {"d6304e01-f8f5-4f45-8b15-d024a6296789", {
    }},
    {"d68dbd8a-3374-4b81-9972-3ec30682db3d", {
    }},
    {"d6942081-d53b-443d-ad47-5e059d9cd27a", {
    }},
    {"d6b5b883-18bd-4b4d-b2ec-9e38affeda82", {
    }},
    {"d6cf9145-d365-471b-bcb8-f0b4a96b891c", {
    }},
    {"d7313ff1-a77a-401c-8c99-3dbdd68add36", {
    }},
    {"d76e7ba8-dfa6-48e7-9670-d62dfb07206b", {
    }},
    {"d7750ee0-c6a4-48ec-b53e-b87b52e6d073", {
    }},
    {"d7b61c70-6323-49cd-a5fc-c84277162c97", {
    }},
    {"d98be98b-b86b-4095-bf52-9d23b2e0a752", {
    }},
    {"d9c22960-532c-4bc6-9876-7b12b52593d7", {
    }},
    {"da520e51-f4e9-4739-ac82-02e0a95c9030", {
    }},
    {"da5d0862-6e76-4e1b-babd-70021bd25494", {
    }},
    {"dc54fd2e-189d-4871-aa01-08c2f57a4abc", {
    }},
    {"dc5877c7-225f-45f7-bac7-e81334b6130a", {
    }},
    {"dc8f80bd-af1e-4289-85b6-3dfc1b493992", {
    }},
    {"dccb10af-b4e2-4b88-95f9-031b4d5ab490", {
    }},
    {"dce33a78-aa18-4b3d-b1df-a6621ac8bdd2", {
    }},
    {"dd141766-313a-4a30-90f0-056a7c968437", {
    }},
    {"ddd1460f-c0bf-4553-8ce4-10433c908fb0", {
    }},
    {"de00de32-547e-4981-ad4b-542f2e9007d8", {
    }},
    {"de35258c-c695-4cbc-b982-38b0ad24ced0", {
    }},
    {"de41cc29-6971-4290-b472-f59f2e2f31e2", {
    }},
    {"de5ef3c7-46e1-484e-9999-62c5308394c1", {
    }},
    {"de621b8f-e125-43a3-a32d-5665446d632a", {
    }},
    {"de9e220b-41d4-4690-8b6b-3d89e231eef1", {
    }},
    {"dea7c82c-1d89-4a66-9427-a4e3debabcb1", {
    }},
    {"debda43a-37b3-4383-91e7-4498da2995ab", {
    }},
    {"deeb2db5-0696-4ce0-94fe-a01f77a45fb5", {
    }},
    {"df975fd3-250a-4004-858f-34e29a3e37aa", {
    }},
    {"dfb9a04d-362f-4ca3-b30b-0254b17b5b84", {
    }},
    {"e08805c8-e395-40df-80d2-54f0d6c43154", {
    }},
    {"e1277516-2b5f-4869-89b1-2e585bd38b7a", {
    }},
    {"e13d8975-81c7-4948-ae3f-37cae11e8ff7", {
    }},
    {"e1a9a38b-6685-46bd-875e-570dc7ad7320", {
    }},
    {"e1ad4953-a752-443c-93bf-80c7525566c2", {
    }},
    {"e1d4a09e-d758-4cd1-b6ec-34a8b5a73f80", {
    }},
    {"e2d40928-632c-4280-a202-e0c2ad1ea0f4", {
    }},
    {"e32596b0-1163-4e02-867a-12132db4ba06", {
    }},
    {"e3690a87-0fa8-4a2a-9a9f-fce8827055ac", {
    }},
    {"e3a7d2c1-80fc-4b40-8f34-30ea111bdc2e", {
    }},
    {"e4f10a3c-49e6-405d-8288-a23bd4eeaa6c", {
    }},
    {"e53d799d-0f3f-466e-b2ff-74634a3cb7a4", {
    }},
    {"e5473742-4611-4aaf-9c49-a3417748cbc8", {
    }},
    {"e55fc3b0-2b60-4220-918e-b21e8bf16016", {
    }},
    {"e6822fee-8c17-4d62-823c-8e9cfcbd1d5c", {
    }},
    {"e6c3d9ad-7b32-4efe-a167-0a868ffdf3af", {
    }},
    {"e6ddcaf7-29c5-4f0a-9a68-d19412ec7090", {
    }},
    {"e77e90df-6271-4f5b-834f-2dd1f245dda4", {
    }},
    {"e7b33238-6584-4170-a5c0-ac25efd9da56", {
    }},
    {"e7c3fb29-caa7-4f47-8c8b-be59b330d4c5", {
    }},
    {"e8309b6e-084c-49b4-b1fc-90a80331b638", {
    }},
    {"e88dcce0-b7b3-11d1-a9f0-00aa0060fa31", {
    }},
    {"e92a2496-223b-4463-a4e3-30eabba79d80", {
    }},
    {"e9641eff-af25-4db7-947b-4128929f8ef5", {
    }},
    {"e9edd392-0b4c-4cf2-82c0-b0d139666245", {
    }},
    {"ea810849-87ff-4b54-abd6-5b71adf466f8", {
    }},
    {"ec0b4191-ab0b-4c66-90b6-c6637cdebbab", {
    }},
    {"ecf4b6f6-d5a6-433c-bb92-4076650fc890", {
    }},
    {"ecf7f4c9-544f-4d6d-9d98-8ad79adaf453", {
    }},
    {"ed4df2d3-8695-450b-856f-f5c1c53acb66", {
    }},
    {"ee31306c-fb9b-4d62-8621-3575d972a9f9", {
    }},
    {"ee3d3d8a-5381-4cfa-b13b-aaf66b5f4ec9", {
    }},
    {"eec7b761-6f94-41b1-949f-c729720dd13c", {
    }},
    {"ef1167eb-cbfc-4341-a568-a7c91a68982c", {
    }},
    {"ef884c5b-2bfe-41bb-aae5-76eedf4f9902", {
    }},
    {"f04bef95-c585-4197-a2b7-df46fdc9ee6d", {
    }},
    {"f0f7984d-222e-4ad2-82ab-1dd8ea40e57e", {
    }},
    {"f1176dfe-7138-4640-8b4c-ae375dc70a6d", {
    }},
    {"f18dedf3-337f-42c0-9e03-cee08708a8c3", {
    }},
    {"f1a24aa7-9ca7-40f6-89ec-97def9ffe8db", {
    }},
    {"f1fdb4af-f78c-466c-bb05-56e92db0b8ec", {
    }},
    {"f21d9941-81f0-471a-adee-4e74b49217ed", {
    }},
    {"f2275480-f782-4291-bd94-f13693513aec", {
    }},
    {"f23f425c-71a1-4fa8-922f-678ea4a60408", {
    }},
    {"f271c659-7e5e-471f-ba25-7f77b286f836", {
    }},
    {"f27abe3a-7111-4dda-8cb2-29222ae23566", {
    }},
    {"f334115e-da1b-4509-9b3d-119504dc7abb", {
    }},
    {"f3713ada-90e3-4e11-aae5-fdc17685b9be", {
    }},
    {"f3aecac4-5b8d-436a-ad0c-64ab194fdaf3", {
    }},
    {"f3c9b698-be85-47ce-888f-83874d9abcb4", {
    }},
    {"f3d8f40d-50cb-44a2-9718-40cb9119495d", {
    }},
    {"f50d2f5d-dda0-48d4-8d2b-e83729fb69a4", {
    }},
    {"f6272d18-cecc-40b1-b26a-3911717aa7bd", {
    }},
    {"f628fd8c-7ba8-465a-a65b-c5aa79263a9e", {
    }},
    {"f7db74b4-4287-4103-afba-f1b13dcd75cf", {
    }},
    {"f8245476-2ec6-44be-b2f7-82ec2537fa2e", {
    }},
    {"f85bf840-a925-4bc2-b0c4-8e36b598679e", {
    }},
    {"f8d3f6ac-4874-42cb-be59-ab454b30716a", {
    }},
    {"f8fa7fa3-d12b-4785-8a4e-691a94f7a3e7", {
    }},
    {"fa303353-b659-4052-85e9-bcac79549b84", {
    }},
    {"fa304789-00c7-4d80-904a-1e4dcc7265aa", {
    }},
    {"fb1de864-e06d-47f4-82a6-8a0aef44493c", {
    }},
    {"fb3842cd-9e2a-4f83-8fcc-4b0761139ae9", {
    }},
    {"fc6976db-8349-4970-ae97-b3c5316a08f0", {
    }},
    {"fc9f7306-ff8f-4d49-9fb6-3ffe5c0951ec", {
    }},
    {"fcad3d3d-0858-400f-aaa3-2f66cce2a6bc", {
    }},
    {"fcc16823-baed-4f24-9b32-a0982117f7fa", {
    }},
    {"fceff153-e839-4cf3-a9e7-ea22832094b8", {
    }},
    {"fcfb52aa-c1e5-4cd8-88bc-f80fd7390f20", {
    }},
    {"fd122953-fa93-4ef7-92c3-04c946b2f7c8", {
    }},
    {"fd9d9fc7-38ec-436d-8fc6-ec39bad301e6", {
    }},
    {"fdf84370-031a-4add-9e91-0d775f1c6605", {
    }},
    {"fe83bb35-4d1a-42e2-916b-06f3e1af719e", {
    }},
    {"fe9e4c12-aacb-4aa3-966d-91a29e6128b5", {
    }},
    {"fec690b7-5f30-4646-ae47-4caafba884a3", {
    }},
    {"fec7952b-4bf0-4c03-b6e1-2796818b7ca9", {
    }},
    {"ff1167eb-cbfc-4341-a568-a7c91a68982c", {
    }},
    {"ff962609-b7d6-4999-862d-95180d529aea", {
    }},
    {"ffae9db7-1c8d-43ff-818c-84403aa3732d", {
    }},
};

std::string GetPropertyDescription(const std::string& guid, int key) {
    std::string lowerGuid = guid;
    for (auto& c : lowerGuid) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    auto it = g_propertyStoreInfo.find(lowerGuid);
    if (it != g_propertyStoreInfo.end()) {
        for (const auto& p : it->second) {
            if (p.first == key) return p.second;
        }
    }
    return "";
}

std::string GetFolderNameFromGuid(const std::string& guid) {
    static const std::unordered_map<std::string, std::string> folderMap = {
        {"1ac14e77-02e7-4e5d-b744-2eb1ae5198b7", "System32"},
        {"374de290-123f-4565-9164-39c4925e467b", "Downloads"},
        {"3dfdf296-dbec-4fb4-81d1-6a3438bcf4de", "Music"},
        {"4bd8d571-6d19-48d3-be97-422220080e43", "Music"},
        {"5e6c858f-0e22-4760-9afe-ea3317b67173", "Music"},
        {"754ac992-0486-4331-8d5c-771f9c2b73b4", "Users"},
        {"7c5a40ef-a0fb-4bfc-874a-c0f2e0b9fa8e", "Program Files (x86)"},
        {"905e63b6-c1bf-494e-b29c-65b732d3d21a", "Program Files"},
        {"a305ce99-f527-492b-8b1a-7e76fa98d6e4", "Installed Updates"},
        {"a4115719-d62e-491d-aa7c-e74b8be3b067", "Start Menu"},
        {"a63293e8-664e-48db-a079-df759e0509f7", "Games"},
        {"a77f5d77-2e2b-44c3-a6a2-aba601054a51", "Start Menu"},
        {"b94237e7-57ac-4347-9151-b08c6c32d1f7", "Templates"},
        {"bfb9d5e0-c6a9-404c-b2b2-ae6db6af4968", "Links"},
        {"c1bae2d0-10df-4334-bedd-7aa20b227a9d", "OEM Links"},
        {"d20beec4-5ca8-4905-ae3b-bf251ea09b53", "Network"},
        {"de74b3b7-1ad9-4e3b-98f3-7e6e2e1eb351", "History"},
        {"df7266ac-9274-4867-8d55-3bd661de872d", "Programs and Features"},
        {"f38bf404-1d43-42f2-9305-67de0b28fc23", "Windows"},
    };
    std::string lowerGuid = guid;
    for (auto& c : lowerGuid) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    auto it = folderMap.find(lowerGuid);
    if (it != folderMap.end()) return it->second;
    return "";
}

} // namespace lecmd
