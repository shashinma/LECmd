#pragma once

#include "CsvWriter.h"
#include <string>
#include <vector>

namespace lecmd {

class HtmlWriter {
public:
    bool Write(const std::string& path, const std::vector<CsvOut>& entries);
};

} // namespace lecmd
