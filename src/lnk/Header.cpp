#include "Header.h"
#include <sstream>

namespace lecmd {

std::string LnkHeader::GetDataFlagsString() const {
    std::vector<std::string> flags;
    if (HasFlag(HasTargetIdList))     flags.push_back("HasTargetIdList");
    if (HasFlag(HasLinkInfo))         flags.push_back("HasLinkInfo");
    if (HasFlag(HasName))             flags.push_back("HasName");
    if (HasFlag(HasRelativePath))     flags.push_back("HasRelativePath");
    if (HasFlag(HasWorkingDir))       flags.push_back("HasWorkingDir");
    if (HasFlag(HasArguments))        flags.push_back("HasArguments");
    if (HasFlag(HasIconLocation))     flags.push_back("HasIconLocation");
    if (HasFlag(IsUnicode))           flags.push_back("IsUnicode");
    if (HasFlag(ForceNoLinkInfo))     flags.push_back("ForceNoLinkInfo");
    if (HasFlag(HasExpString))        flags.push_back("HasExpString");
    if (HasFlag(RunInSeparateProcess))flags.push_back("RunInSeparateProcess");
    if (HasFlag(HasDarwinId))         flags.push_back("HasDarwinId");
    if (HasFlag(RunAsUser))           flags.push_back("RunAsUser");
    if (HasFlag(HasExpIcon))          flags.push_back("HasExpIcon");
    if (HasFlag(NoPidlAlias))         flags.push_back("NoPidlAlias");
    if (HasFlag(RunWithShimLayer))    flags.push_back("RunWithShimLayer");
    if (HasFlag(ForceNoLinkTrack))    flags.push_back("ForceNoLinkTrack");
    if (HasFlag(EnableTargetMetadata))flags.push_back("EnableTargetMetadata");

    std::ostringstream oss;
    for (size_t i = 0; i < flags.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << flags[i];
    }
    return oss.str();
}

std::string LnkHeader::GetFileAttributesString() const {
    std::vector<std::string> attrs;
    if (HasFileAttr(FileAttributeReadonly))        attrs.push_back("Readonly");
    if (HasFileAttr(FileAttributeHidden))          attrs.push_back("Hidden");
    if (HasFileAttr(FileAttributeSystem))          attrs.push_back("System");
    if (HasFileAttr(FileAttributeDirectory))       attrs.push_back("Directory");
    if (HasFileAttr(FileAttributeArchive))         attrs.push_back("Archive");
    if (HasFileAttr(FileAttributeDevice))          attrs.push_back("Device");
    if (HasFileAttr(FileAttributeNormal))          attrs.push_back("Normal");
    if (HasFileAttr(FileAttributeTemporary))       attrs.push_back("Temporary");
    if (HasFileAttr(FileAttributeSparseFile))      attrs.push_back("SparseFile");
    if (HasFileAttr(FileAttributeReparsePoint))    attrs.push_back("ReparsePoint");
    if (HasFileAttr(FileAttributeCompressed))      attrs.push_back("Compressed");
    if (HasFileAttr(FileAttributeOffline))         attrs.push_back("Offline");
    if (HasFileAttr(FileAttributeNotContentIndexed))attrs.push_back("NotContentIndexed");
    if (HasFileAttr(FileAttributeEncrypted))       attrs.push_back("Encrypted");
    if (HasFileAttr(UnkWin95Fat))                  attrs.push_back("UnkWin95Fat");
    if (HasFileAttr(FileAttributeVirtual))         attrs.push_back("Virtual");

    std::ostringstream oss;
    for (size_t i = 0; i < attrs.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << attrs[i];
    }
    return oss.str();
}

std::string LnkHeader::GetShowWindowString() const {
    switch (showWindow) {
        case 0:  return "Hides the window and activates another window";
        case 1:  return "Activates and displays a window";
        case 2:  return "Activates and minimizes the window";
        case 3:  return "Activates and maximizes the window";
        case 4:  return "Display the window in its most recent position and size without activating it";
        case 5:  return "Activates the window and displays it in its current size and position";
        case 6:  return "Minimizes the window and activates the next top-level window";
        case 7:  return "Display the window as minimized without activating it";
        case 8:  return "Display the window in its current size and position without activating it";
        case 9:  return "Restores the window";
        case 10: return "Set the show state based on the ShowWindow values";
        case 11: return "Minimizes a window";
        default: return "Unknown";
    }
}

std::string LnkHeader::GetHotKeyString() const {
    if (hotKey == 0) return "";
    uint8_t low = hotKey & 0xFF;
    uint8_t high = (hotKey >> 8) & 0xFF;

    std::string hk;
    switch (high) {
        case 1: hk = "SHIFT+"; break;
        case 2: hk = "CONTROL+"; break;
        case 4: hk = "ALT+"; break;
        case 6: hk = "CONTROL+ALT+"; break;
    }

    if (low >= 0x30 && low <= 0x39) {
        hk += static_cast<char>(low);
    } else if (low >= 0x41 && low <= 0x5A) {
        hk += static_cast<char>(low);
    } else if (low >= 0x70 && low <= 0x87) {
        hk += "F" + std::to_string(low - 111);
    } else if (low == 0x90) {
        hk += "NUMLOCK";
    } else if (low == 0x91) {
        hk += "SCROLLLOCK";
    }
    return hk;
}

} // namespace lecmd
