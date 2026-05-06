# LECmd

Cross-platform implementation of the Windows LNK file parser.

This project is a fully compatible, cross-platform alternative to the original .NET LECmd, designed to parse Windows shortcut (`.lnk`) files and extract forensic artifacts on any operating system.

## Overview

LECmd reads Windows `.lnk` files — shell shortcuts that contain rich metadata about target files, including timestamps, file paths, volume information, network share details, command-line arguments, and more. This implementation produces the same console output and file exports as the reference implementation, making it suitable for forensic analysis and incident response workflows.

## Key Features

- **Cross-platform** — works on Windows, macOS, and Linux
- **Format compatible** — identical CSV (27 columns), JSON, XML, and HTML output structure
- **Complete LNK parsing** — Header, TargetIDList, LinkInfo, StringData, and all ExtraData blocks
- **PropertyStore support** — fully parses serialized property sheets with VT type handlers and 536+ GUID lookups
- **Tracker database** — extracts MAC address, machine ID, and creation time from DROID GUIDs
- **ExtensionBlocks** — handles BEEF0003, BEEF0004, BEEF001A, BEEF0025 signatures
- **ShellItem coverage** — 15+ shell bag types including ZipContents
- **No external dependencies** — all third-party libraries (CLI11, fmt, spdlog, nlohmann/json) are vendored in `third_party/`
- **Stand-alone** — single binary with no runtime installation requirements
- **CMake-based build** — simple `mkdir build && cmake .. && make` workflow

## Building

```bash
make build
```

The binary will be available at `build/LECmd`.

## Usage

Process a single file:

```bash
./LECmd -f /path/to/file.lnk --csv /output/directory
```

Recursively process a directory:

```bash
./LECmd -d /path/to/shortcuts/ --csv /output/directory
```

Export all formats at once:

```bash
./LECmd -f /path/to/file.lnk --csv /out --json /out --xml /out --html /out
```

All standard options are supported: quiet mode, no extra blocks, no target IDs, microsecond precision, custom codepage, removable-only filtering, and more.
