cmake_minimum_required(VERSION 3.16)
project(LECmd VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Vendored dependencies
add_subdirectory(third_party/cli11 EXCLUDE_FROM_ALL)
add_subdirectory(third_party/fmt EXCLUDE_FROM_ALL)
add_subdirectory(third_party/spdlog EXCLUDE_FROM_ALL)

# nlohmann/json is header-only
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json)

set(LECMD_SOURCES
    src/lnk/LnkFile.cpp
    src/lnk/Header.cpp
    src/lnk/VolumeInfo.cpp
    src/lnk/LinkInfo.cpp
    src/lnk/ShellItems.cpp
    src/lnk/ExtraData.cpp
    src/lnk/ExtensionBlocks.cpp
    src/lnk/PropertyStore.cpp
    src/output/CsvWriter.cpp
    src/output/JsonWriter.cpp
    src/output/XmlWriter.cpp
    src/output/HtmlWriter.cpp
    src/utils/DateTimeUtils.cpp
    src/utils/MacVendor.cpp
    src/utils/StringUtils.cpp
)

set(LECMD_HEADERS
    src/lnk/LnkFile.h
    src/lnk/Header.h
    src/lnk/VolumeInfo.h
    src/lnk/LinkInfo.h
    src/lnk/ShellItems.h
    src/lnk/ExtraData.h
    src/lnk/ExtensionBlocks.h
    src/lnk/PropertyStore.h
    src/output/CsvWriter.h
    src/output/JsonWriter.h
    src/output/XmlWriter.h
    src/output/HtmlWriter.h
    src/utils/DateTimeUtils.h
    src/utils/MacVendor.h
    src/utils/StringUtils.h
)

add_library(lecmd_lib STATIC ${LECMD_SOURCES} ${LECMD_HEADERS})

target_include_directories(lecmd_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(lecmd_lib
    PUBLIC
        spdlog::spdlog
        fmt::fmt
        nlohmann_json
)

if(WIN32)
    target_compile_definitions(lecmd_lib PRIVATE _WIN32_WINNT=0x0601 NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Find iconv for codepage string decoding (macOS/Linux)
find_package(Iconv)
if(Iconv_FOUND)
    target_link_libraries(lecmd_lib PUBLIC Iconv::Iconv)
endif()

if(MSVC)
    target_compile_options(lecmd_lib PRIVATE /W4 /WX-)
else()
    target_compile_options(lecmd_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(LECmd src/main.cpp)

target_link_libraries(LECmd PRIVATE lecmd_lib CLI11::CLI11)

if(MSVC)
    target_compile_options(LECmd PRIVATE /W4 /WX-)
else()
    target_compile_options(LECmd PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS LECmd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
