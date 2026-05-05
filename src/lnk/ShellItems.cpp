#include "ShellItems.h"

namespace lecmd {

std::string ShellBag::GetTypeName() const {
    switch (type) {
        case 0x00: return "Root folder";
        case 0x01: return "Volume";
        case 0x1f: return "Drive / Computer";
        case 0x23: return "GUID";
        case 0x2e: return "Device";
        case 0x2f: return "Volume (0x2f)";
        case 0x31: return "Directory";
        case 0x32: return "File";
        case 0x36: return "File (0x36)";
        case 0x40: return "Network";
        case 0x61: return "URI";
        case 0x71: return "Control Panel";
        case 0x74: return "Delegate Item";
        case 0xc3: return "Users Property View";
        default: return "Unknown";
    }
}

std::string Beef0004Block::GetOsHint() const {
    switch (identifier) {
        case 0x14: return "Windows XP, 2003";
        case 0x26: return "Windows Vista";
        case 0x2a: return "Windows 2008, 7, 8";
        case 0x2e: return "Windows 8.1, 10";
        default: return "Unknown operating system";
    }
}

std::string ParseShellBagAbsolutePath(const std::vector<std::shared_ptr<ShellBag>>& bags) {
    std::string path;
    for (const auto& bag : bags) {
        if (!bag->value.empty()) {
            if (!path.empty() && path.back() != '\\') path += '\\';
            std::string v = bag->value;
            while (!v.empty() && v.front() == '\\') v = v.substr(1);
            while (!v.empty() && v.back() == '\\') v.pop_back();
            path += v;
        }
    }
    return path;
}

} // namespace lecmd
