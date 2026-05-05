#pragma once

#include <cstdint>
#include <string>

namespace lecmd {

struct VolumeInfo {
    enum DriveType : uint32_t {
        DriveUnknown    = 0,
        DriveNoRootDir  = 1,
        DriveRemovable  = 2,
        DriveFixed      = 3,
        DriveRemote     = 4,
        DriveCdrom      = 5,
        DriveRamdisk    = 6
    };

    int32_t size = 0;
    DriveType driveType = DriveUnknown;
    std::string volumeSerialNumber;
    int32_t volumeLabelOffset = 0;
    std::string volumeLabel;

    std::string GetDriveTypeString() const;
};

} // namespace lecmd
