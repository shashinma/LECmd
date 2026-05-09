#include "StringUtils.h"
#include <iconv.h>

namespace lecmd {

std::string DecodeCp1252(const uint8_t* data, size_t len) {
    if (len == 0) return {};
    iconv_t cd = iconv_open("UTF-8", "CP1252");
    if (cd == (iconv_t)-1) {
        return std::string(reinterpret_cast<const char*>(data), len);
    }
    std::string out;
    out.resize(len * 3);
    char* inbuf = const_cast<char*>(reinterpret_cast<const char*>(data));
    char* outbuf = &out[0];
    size_t inbytes = len;
    size_t outbytes = out.size();
    size_t res = iconv(cd, &inbuf, &inbytes, &outbuf, &outbytes);
    iconv_close(cd);
    if (res == (size_t)-1) {
        size_t converted = out.size() - outbytes;
        out.resize(converted);
        if (inbytes > 0 && inbuf != nullptr) {
            out.append(reinterpret_cast<const char*>(inbuf), inbytes);
        }
        return out;
    }
    out.resize(out.size() - outbytes);
    return out;
}

} // namespace lecmd
