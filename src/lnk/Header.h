#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace lecmd {

struct LnkHeader {
    // Shell Link Header (76 bytes)
    uint32_t headerSize = 0;
    // CLSID {00021401-0000-0000-C000-000000000046}
    uint8_t clsid[16]{};

    // Link flags
    uint32_t dataFlags = 0;
    // File attributes
    uint32_t fileAttributes = 0;

    // Target file timestamps (FILETIME)
    uint64_t targetCreationTime = 0;
    uint64_t targetAccessTime = 0;
    uint64_t targetModificationTime = 0;

    uint32_t fileSize = 0;
    int32_t iconIndex = 0;
    uint32_t showWindow = 0;
    uint16_t hotKey = 0;
    uint16_t reserved0 = 0;
    uint32_t reserved1 = 0;
    uint32_t reserved2 = 0;

    enum DataFlag : uint32_t {
        HasTargetIdList     = 0x00000001,
        HasLinkInfo         = 0x00000002,
        HasName             = 0x00000004,
        HasRelativePath     = 0x00000008,
        HasWorkingDir       = 0x00000010,
        HasArguments        = 0x00000020,
        HasIconLocation     = 0x00000040,
        IsUnicode           = 0x00000080,
        ForceNoLinkInfo     = 0x00000100,
        HasExpString        = 0x00000200,
        RunInSeparateProcess= 0x00000400,
        HasDarwinId         = 0x00001000,
        RunAsUser           = 0x00002000,
        HasExpIcon          = 0x00004000,
        NoPidlAlias         = 0x00008000,
        RunWithShimLayer    = 0x00020000,
        ForceNoLinkTrack    = 0x00040000,
        EnableTargetMetadata= 0x00080000,
    };

    enum FileAttribute : uint32_t {
        FileAttributeReadonly       = 0x00000001,
        FileAttributeHidden         = 0x00000002,
        FileAttributeSystem         = 0x00000004,
        ResVolumeLabel              = 0x00000008,
        FileAttributeDirectory      = 0x00000010,
        FileAttributeArchive        = 0x00000020,
        FileAttributeDevice         = 0x00000040,
        FileAttributeNormal         = 0x00000080,
        FileAttributeTemporary      = 0x00000100,
        FileAttributeSparseFile     = 0x00000200,
        FileAttributeReparsePoint   = 0x00000400,
        FileAttributeCompressed     = 0x00000800,
        FileAttributeOffline        = 0x00001000,
        FileAttributeNotContentIndexed= 0x00002000,
        FileAttributeEncrypted      = 0x00004000,
        UnkWin95Fat                 = 0x00008000,
        FileAttributeVirtual        = 0x00010000,
    };

    bool HasFlag(DataFlag flag) const { return (dataFlags & flag) != 0; }
    bool HasFileAttr(FileAttribute attr) const { return (fileAttributes & attr) != 0; }

    std::string GetDataFlagsString() const;
    std::string GetFileAttributesString() const;
    std::string GetShowWindowString() const;
    std::string GetHotKeyString() const;
};

} // namespace lecmd
