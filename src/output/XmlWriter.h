#pragma once

#include "CsvWriter.h"
#include <string>
#include <vector>

namespace lecmd {

class XmlWriter {
public:
    bool Write(const std::string& path, const std::vector<CsvOut>& entries);
    bool WriteSingle(const std::string& path, const CsvOut& entry);
};

} // namespace lecmd
