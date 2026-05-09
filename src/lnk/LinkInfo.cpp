#include "LinkInfo.h"
#include <vector>

namespace lecmd {

std::string NetworkShareInfo::GetProviderTypeString() const {
    switch (networkProviderType) {
        case 0x00020000: return "WnncNetLanman";
        case 0x00030000: return "WnncNetNetware";
        case 0x00040000: return "WnncNetVines";
        case 0x00050000: return "WnncNet10Net";
        case 0x00060000: return "WnncNetLocus";
        case 0x00070000: return "WnncNetSunPcNfs";
        case 0x00080000: return "WnncNetLanstep";
        case 0x00090000: return "WnncNet_9Tiles";
        case 0x000A0000: return "WnncNetLantastic";
        case 0x000B0000: return "WnncNetAs400";
        case 0x000C0000: return "WnncNetFtpNfs";
        case 0x000D0000: return "WnncNetPathworks";
        case 0x000E0000: return "WnncNetLifenet";
        case 0x000F0000: return "WnncNetPowerlan";
        case 0x00100000: return "WnncNetBwnfs";
        case 0x00110000: return "WnncNetCogent";
        case 0x00120000: return "WnncNetFarallon";
        case 0x00130000: return "WnncNetAppletalk";
        case 0x00140000: return "WnncNetIntergraph";
        case 0x00150000: return "WnncNetSymfonet";
        case 0x00160000: return "WnncNetClearcase";
        case 0x00170000: return "WnncNetFrontier";
        case 0x00180000: return "WnncNetBmc";
        case 0x00190000: return "WnncNetDce";
        case 0x001A0000: return "WnncNetAvid";
        case 0x001B0000: return "WnncNetDocuspace";
        case 0x001C0000: return "WnncNetMangosoft";
        case 0x001D0000: return "WnncNetSernet";
        case 0x001E0000: return "WnncNetRiverfront1";
        case 0x001F0000: return "WnncNetRiverfront2";
        case 0x00200000: return "WnncNetDecorb";
        case 0x00210000: return "WnncNetProtstor";
        case 0x00220000: return "WnncNetFjRedir";
        case 0x00230000: return "WnncNetDistinct";
        case 0x00240000: return "WnncNetTwins";
        case 0x00250000: return "WnncNetRdr2Sample";
        case 0x00260000: return "WnncNetCsc";
        case 0x00270000: return "WnncNet_3In1";
        case 0x00280000: return "WnncNetExtendnet";
        case 0x00290000: return "WnncNetStac";
        case 0x002A0000: return "WnncNetFoxbat";
        case 0x002B0000: return "WnncNetYahoo";
        case 0x002C0000: return "WnncNetExifs";
        case 0x002D0000: return "WnncNetDav";
        case 0x002E0000: return "WnncNetKnoware";
        case 0x002F0000: return "WnncNetObjectDire";
        case 0x00300000: return "WnncNetMasfax";
        case 0x00310000: return "WnncNetHobNfs";
        case 0x00320000: return "WnncNetShiva";
        case 0x00330000: return "WnncNetIbmal";
        case 0x00340000: return "WnncNetLock";
        case 0x00350000: return "WnncNetTermsrv";
        case 0x00360000: return "WnncNetSrt";
        case 0x00370000: return "WnncNetQuincy";
        case 0x00380000: return "WnncNetOpenafs";
        case 0x00390000: return "WnncNetAvid1";
        case 0x003A0000: return "WnncNetDfs";
        case 0x003B0000: return "WnncNetKwnp";
        case 0x003C0000: return "WnncNetZenworks";
        case 0x003D0000: return "WnncNetDriveonweb";
        case 0x003E0000: return "WnncNetVmware";
        case 0x003F0000: return "WnncNetRsfx";
        case 0x00400000: return "WnncNetMfiles";
        case 0x00410000: return "WnncNetMsNfs";
        case 0x00420000: return "WnncNetGoogle";
        default: return "Unknown";
    }
}

std::string NetworkShareInfo::GetShareFlagsString() const {
    std::vector<std::string> f;
    if (flags & 0x00000001) f.push_back("ValidDevice");
    if (flags & 0x00000002) f.push_back("ValidNetType");
    std::string res;
    for (size_t i = 0; i < f.size(); ++i) {
        if (i > 0) res += ", ";
        res += f[i];
    }
    return res.empty() ? "None" : res;
}

std::string LinkInfo::GetFlagsString() const {
    switch (flags) {
        case 0x1: return "VolumeIdAndLocalBasePath";
        case 0x2: return "CommonNetworkRelativeLinkAndPathSuffix";
        case 0x3: return "VolumeIdAndLocalBasePath, CommonNetworkRelativeLinkAndPathSuffix";
        default: return "Unknown (0x" + std::to_string(flags) + ")";
    }
}

} // namespace lecmd
