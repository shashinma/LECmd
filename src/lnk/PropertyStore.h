#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <cstdint>

namespace lecmd {

struct PropertySheet {
    std::string guid;
    std::vector<std::tuple<std::string, std::string, std::string>> properties;
    // (key, description, value)
};

class PropertyStore {
public:
    explicit PropertyStore(const std::vector<uint8_t>& rawBytes);
    std::vector<PropertySheet> sheets;
};

std::string GetPropertyDescription(const std::string& guid, int key);
std::string GetFolderNameFromGuid(const std::string& guid);

} // namespace lecmd
