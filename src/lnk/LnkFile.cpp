#include "LnkFile.h"
#include <fstream>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>

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

static std::string ReadAsciiString(const uint8_t* data, size_t& offset, size_t maxLen = 1024) {
    std::string result;
    for (size_t i = 0; i < maxLen && data[offset + i] != 0; ++i) {
        result += static_cast<char>(data[offset + i]);
    }
    while (data[offset] != 0 && offset < maxLen + offset) ++offset;
    if (data[offset] == 0) ++offset;
    return result;
}

static std::string ReadUnicodeString(const uint8_t* data, size_t& offset, size_t count) {
    std::string result;
    for (size_t i = 0; i < count; ++i) {
        uint16_t ch = ReadUInt16LE(data, offset + i * 2);
        if (ch == 0) break;
        if (ch < 0x80) {
            result += static_cast<char>(ch);
        } else {
            // Simple UTF-16 to UTF-8 for BMP
            if (ch < 0x800) {
                result += static_cast<char>(0xC0 | (ch >> 6));
                result += static_cast<char>(0x80 | (ch & 0x3F));
            } else {
                result += static_cast<char>(0xE0 | (ch >> 12));
                result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (ch & 0x3F));
            }
        }
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
        auto bag = ParseShellItem(itemData, 1252);
        if (bag) {
            targetIDs.push_back(bag);
        }
        index += itemSize;
    }
    // Ensure index is at listEnd (skips terminal ID 0x0000 and any padding)
    index = listEnd;
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
            bag->friendlyName = "Drive / Computer";
            if (len >= 20) {
                uint8_t flags = data[3];
                std::string clsid = GuidToString(data + 4);
                bag->value = clsid;
            }
            break;
        }
        case 0x22:
        case 0x23: {
            bag->friendlyName = "GUID";
            if (len >= 18) {
                bag->value = GuidToString(data + 3);
            }
            break;
        }
        case 0x2a:
        case 0x2f: {
            bag->friendlyName = "Volume";
            if (len >= 4) {
                size_t off = 3;
                bag->value = ReadAsciiString(data, off);
            }
            break;
        }
        case 0x2e: {
            bag->friendlyName = "Device";
            if (len >= 20) {
                bag->value = GuidToString(data + 4);
            }
            break;
        }
        case 0xb1:
        case 0x31:
        case 0x3A:
        case 0x35:
        case 0x39: {
            bag->friendlyName = "Directory";
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
                while (off + nameLen < len && data[off + nameLen] != 0) {
                    nameLen++;
                }
                // Cap name length at beefPos if found
                if (beefPos > 0 && static_cast<size_t>(beefPos) > off && nameLen > static_cast<size_t>(beefPos) - off) {
                    nameLen = beefPos - off;
                }

                if (nameLen > 0 && off + nameLen <= len) {
                    std::string shortName;
                    for (size_t i = 0; i < nameLen; ++i) {
                        shortName += static_cast<char>(data[off + i]);
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

                size_t nameLen = 0;
                while (off + nameLen < len && data[off + nameLen] != 0) {
                    nameLen++;
                }
                if (beefPos > 0 && static_cast<size_t>(beefPos) > off && nameLen > static_cast<size_t>(beefPos) - off) {
                    nameLen = beefPos - off;
                }

                if (nameLen > 0 && off + nameLen <= len) {
                    std::string shortName;
                    for (size_t i = 0; i < nameLen; ++i) {
                        shortName += static_cast<char>(data[off + i]);
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
            bag->friendlyName = "Root folder";
            if (len >= 20) {
                uint8_t flags = data[3];
                bag->value = GuidToString(data + 4);
            }
            break;
        }
        case 0x01: {
            bag->friendlyName = "Volume";
            if (len >= 4) {
                bag->driveLetter = std::string(1, static_cast<char>(data[3])) + ":";
                bag->value = bag->driveLetter;
            }
            break;
        }
        case 0x71: {
            bag->friendlyName = "Control Panel";
            if (len >= 18) {
                bag->value = GuidToString(data + 3);
            }
            break;
        }
        case 0x61: {
            bag->friendlyName = "URI";
            if (len >= 4) {
                size_t off = 3;
                bag->value = ReadAsciiString(data, off);
            }
            break;
        }
        case 0xC3: {
            bag->friendlyName = "Users Property View";
            if (len >= 4) {
                size_t off = 3;
                bag->value = ReadAsciiString(data, off);
            }
            break;
        }
        case 0x74:
        case 0x77: {
            bag->friendlyName = "Delegate Item";
            if (len >= 6) {
                size_t off = 3;
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
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x46:
        case 0x47:
        case 0x49:
        case 0x4a:
        case 0x4b:
        case 0x4c: {
            bag->friendlyName = "Network";
            if (len >= 4) {
                size_t off = 3;
                bag->value = ReadAsciiString(data, off);
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
            off = offset + 18;
            if (version >= 7 && off + 18 <= offset + blockSize) {
                off += 2; // skip empty 2
                // MFT info: 8 bytes
                uint32_t entryLow = ReadUInt32LE(data, off);
                uint16_t entryHigh = ReadUInt16LE(data, off + 4);
                uint16_t seqNum = ReadUInt16LE(data, off + 6);
                uint64_t entryIndex = entryLow;
                if (entryHigh != 0) {
                    entryIndex += (static_cast<uint64_t>(entryHigh) << 24);
                }
                block->mftInfo.mftEntryNumber = entryIndex;
                if (seqNum != 0) {
                    block->mftInfo.mftSequenceNumber = seqNum;
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
                    } else if (ch < 0x80) {
                        current += static_cast<char>(ch);
                    } else if (ch < 0x800) {
                        current += static_cast<char>(0xC0 | (ch >> 6));
                        current += static_cast<char>(0x80 | (ch & 0x3F));
                    } else {
                        current += static_cast<char>(0xE0 | (ch >> 12));
                        current += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                        current += static_cast<char>(0x80 | (ch & 0x3F));
                    }
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
            offset += blockSize;
            return block;
        }
        case 0xBEEF0025: {
            auto block = std::make_shared<Beef0025Block>();
            block->signature = signature;
            offset += blockSize;
            return block;
        }
        default: {
            // Unknown BEEF block, skip it
            offset += blockSize;
            return nullptr;
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

            if (vi->volumeLabelOffset > 16 && volOff + vi->volumeLabelOffset < raw.size()) {
                size_t lblOff = volOff + vi->volumeLabelOffset;
                size_t maxLen = raw.size() - lblOff;
                std::string lbl;
                for (size_t i = 0; i < maxLen && data[lblOff + i] != 0; ++i) {
                    lbl += static_cast<char>(data[lblOff + i]);
                }
                vi->volumeLabel = lbl;
            } else if (volOff + 16 < raw.size()) {
                size_t lblOff = volOff + 16;
                size_t maxLen = raw.size() - lblOff;
                std::string lbl;
                for (size_t i = 0; i < maxLen && data[lblOff + i] != 0; ++i) {
                    lbl += static_cast<char>(data[lblOff + i]);
                }
                vi->volumeLabel = lbl;
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

            if (ni->netNameOffset > 0 && netOff + ni->netNameOffset < raw.size()) {
                size_t nameOff = netOff + ni->netNameOffset;
                size_t maxLen = raw.size() - nameOff;
                std::string name;
                for (size_t i = 0; i < maxLen && data[nameOff + i] != 0; ++i) {
                    name += static_cast<char>(data[nameOff + i]);
                }
                ni->networkShareName = name;
            }
            if (ni->deviceNameOffset > 0 && netOff + ni->deviceNameOffset < raw.size()) {
                size_t devOff = netOff + ni->deviceNameOffset;
                size_t maxLen = raw.size() - devOff;
                std::string dev;
                for (size_t i = 0; i < maxLen && data[devOff + i] != 0; ++i) {
                    dev += static_cast<char>(data[devOff + i]);
                }
                ni->deviceName = dev;
            }
            linkInfo->networkShareInfo = ni;
        }
    }

    // Local base path
    if (linkInfo->localBasePathOffset > 0) {
        size_t pathOff = index + linkInfo->localBasePathOffset;
        if (pathOff < raw.size()) {
            size_t maxLen = raw.size() - pathOff;
            std::string path;
            for (size_t i = 0; i < maxLen && data[pathOff + i] != 0; ++i) {
                path += static_cast<char>(data[pathOff + i]);
            }
            linkInfo->localBasePath = path;
        }
    }

    // Common path suffix
    if (linkInfo->commonPathSuffixOffset > 0) {
        size_t pathOff = index + linkInfo->commonPathSuffixOffset;
        if (pathOff < raw.size()) {
            size_t maxLen = raw.size() - pathOff;
            std::string path;
            for (size_t i = 0; i < maxLen && data[pathOff + i] != 0; ++i) {
                path += static_cast<char>(data[pathOff + i]);
            }
            linkInfo->commonPathSuffix = path;
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
            name = ReadAsciiString(data, index);
        }
    }

    if (header.HasFlag(LnkHeader::HasRelativePath)) {
        if (index + 2 > raw.size()) return;
        uint16_t count = ReadUInt16LE(data, index);
        index += 2;
        if (unicode) {
            relativePath = ReadUnicodeString(data, index, count);
        } else {
            relativePath = ReadAsciiString(data, index);
        }
    }

    if (header.HasFlag(LnkHeader::HasWorkingDir)) {
        if (index + 2 > raw.size()) return;
        uint16_t count = ReadUInt16LE(data, index);
        index += 2;
        if (unicode) {
            workingDirectory = ReadUnicodeString(data, index, count);
        } else {
            workingDirectory = ReadAsciiString(data, index);
        }
    }

    if (header.HasFlag(LnkHeader::HasArguments)) {
        if (index + 2 > raw.size()) return;
        uint16_t count = ReadUInt16LE(data, index);
        index += 2;
        if (unicode) {
            arguments = ReadUnicodeString(data, index, count);
        } else {
            arguments = ReadAsciiString(data, index);
        }
    }

    if (header.HasFlag(LnkHeader::HasIconLocation)) {
        if (index + 2 > raw.size()) return;
        uint16_t count = ReadUInt16LE(data, index);
        index += 2;
        if (unicode) {
            iconLocation = ReadUnicodeString(data, index, count);
        } else {
            iconLocation = ReadAsciiString(data, index);
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

        if (size < 8 || index + size > raw.size()) {
            break;
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
                    block->fontSize = ReadUInt32LE(data, index + 24);
                    block->fontFamily = ReadUInt32LE(data, index + 28);
                    block->fontWeight = ReadUInt32LE(data, index + 32);
                    // Face name at offset 36, up to 32 wchar
                    size_t off = index + 36;
                    block->faceName = ReadUnicodeNullTerminated(data, off, 32);
                    block->cursorSize = ReadUInt32LE(data, index + 0x64);
                    block->fullScreen = ReadUInt32LE(data, index + 0x68);
                    block->quickEdit = ReadUInt32LE(data, index + 0x6C);
                    block->insertMode = ReadUInt32LE(data, index + 0x70);
                    block->autoPosition = ReadUInt32LE(data, index + 0x74);
                    block->historyBufferSize = ReadUInt32LE(data, index + 0x78);
                    block->numberOfHistoryBuffers = ReadUInt32LE(data, index + 0x7C);
                    block->historyNoDup = ReadUInt32LE(data, index + 0x80);
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
                if (size >= 0x158) {
                    size_t off = index + 8;
                    block->applicationIdentifierUnicode = ReadUnicodeNullTerminated(data, off, 260);
                    off = index + 0x210;
                    block->productCode = ReadUnicodeNullTerminated(data, off, 39);
                    off = index + 0x262;
                    block->featureName = ReadUnicodeNullTerminated(data, off, 39);
                    off = index + 0x2B4;
                    block->componentId = ReadUnicodeNullTerminated(data, off, 39);
                }
                extraBlocks.push_back(block);
                break;
            }
            case 0xA0000001: { // EnvironmentVariableDataBlock
                auto block = std::make_shared<EnvironmentVariableDataBlock>();
                block->size = size;
                block->signature = sig;
                if (size >= 0x20A) {
                    size_t off = index + 8;
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
                    size_t off = index + 8;
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
                }
                extraBlocks.push_back(block);
                break;
            }
            case 0xA0000009: { // PropertyStoreDataBlock
                auto block = std::make_shared<PropertyStoreDataBlock>();
                block->size = size;
                block->signature = sig;
                // Property store parsing is complex, skip detailed parsing for now
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

                    // Machine ID: 16 bytes at offset 0x10, null-terminated
                    if (index + 32 <= raw.size()) {
                        std::string mid;
                        for (int i = 0; i < 16; ++i) {
                            char c = static_cast<char>(data[index + 16 + i]);
                            if (c == '\0') break;
                            mid += c;
                        }
                        block->machineId = mid;
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
                        auto bag = ParseShellItem(itemData, 1252);
                        if (bag) block->targetIDs.push_back(bag);
                        listOff += itemSize;
                    }
                }
                extraBlocks.push_back(block);
                break;
            }
            default: {
                // Unknown block, skip
                break;
            }
        }

        index += size;
    }
}

std::string LnkFile::LocalPath() const {
    if (linkInfo && !linkInfo->localBasePath.empty()) {
        return linkInfo->localBasePath;
    }
    return "";
}

std::string LnkFile::CommonPath() const {
    if (linkInfo && !linkInfo->commonPathSuffix.empty()) {
        return linkInfo->commonPathSuffix;
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
