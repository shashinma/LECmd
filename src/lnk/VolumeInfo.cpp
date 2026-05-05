#include "VolumeInfo.h"

namespace lecmd {

std::string VolumeInfo::GetDriveTypeString() const {
    switch (driveType) {
        case DriveUnknown:    return "Unknown";
        case DriveNoRootDir:  return "No root directory";
        case DriveRemovable:  return "Removable storage media (Floppy, USB)";
        case DriveFixed:      return "Fixed storage media (Hard drive)";
        case DriveRemote:     return "Remote storage";
        case DriveCdrom:      return "Optical disc (CD-ROM, DVD, BD)";
        case DriveRamdisk:    return "RAM drive";
        default:              return "Unknown";
    }
}

} // namespace lecmd
