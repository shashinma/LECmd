#pragma once

#include <string>
#include <unordered_map>

namespace lecmd {

class MacVendorLookup {
public:
    MacVendorLookup();
    std::string Lookup(const std::string& macAddress) const;

private:
    std::unordered_map<std::string, std::string> vendors_;
    void LoadDefaults();
};

} // namespace lecmd
