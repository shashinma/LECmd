#pragma once

#include "CsvWriter.h"
#include <string>
#include <vector>

namespace lecmd {

class JsonWriter {
public:
    bool Write(const std::string& path, const std::vector<CsvOut>& entries, bool pretty);
};

} // namespace lecmd
