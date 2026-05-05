#include "LinkInfo.h"
#include <vector>

namespace lecmd {

std::string NetworkShareInfo::GetProviderTypeString() const {
    switch (networkProviderType) {
        case 0x00010000: return "WNNC_NET_AVID";
        case 0x00090000: return "WNNC_NET_LANSTEP";
        case 0x000A0000: return "WNNC_NET_9P";
        case 0x000B0000: return "WNNC_NET_LANMAN";
        case 0x000C0000: return "WNNC_NET_NETWARE";
        case 0x000D0000: return "WNNC_NET_VINES";
        case 0x000E0000: return "WNNC_NET_LNP";
        case 0x00110000: return "WNNC_NET_DOS_PRINT";
        case 0x00120000: return "WNNC_NET_OS2_PRINT";
        case 0x00130000: return "WNNC_NET_POWERLAN";
        case 0x00140000: return "WNNC_NET_APPLETALK";
        case 0x00150000: return "WNNC_NET_INTERGRAPH";
        case 0x00160000: return "WNNC_NET_SYMFONET";
        case 0x00170000: return "WNNC_NET_CLEARCASE";
        case 0x00180000: return "WNNC_NET_FRONTIER";
        case 0x00190000: return "WNNC_NET_BMC";
        case 0x001A0000: return "WNNC_NET_DCE";
        case 0x001B0000: return "WNNC_NET_AVID1";
        case 0x001C0000: return "WNNC_NET_DOCUSPACE";
        case 0x001D0000: return "WNNC_NET_MANGOSOFT";
        case 0x001E0000: return "WNNC_NET_SERNET";
        case 0x001F0000: return "WNNC_NET_RIVERFRONT1";
        case 0x00200000: return "WNNC_NET_RIVERFRONT2";
        case 0x00210000: return "WNNC_NET_DECORB";
        case 0x00220000: return "WNNC_NET_PROTSTOR";
        case 0x00230000: return "WNNC_NET_FJ_REDIR";
        case 0x00240000: return "WNNC_NET_DISTINCT";
        case 0x00250000: return "WNNC_NET_TWINS";
        case 0x00260000: return "WNNC_NET_RDR2SAMPLE";
        case 0x00270000: return "WNNC_NET_CSC";
        case 0x00280000: return "WNNC_NET_3IN1";
        case 0x00290000: return "WNNC_NET_EXTENDNET";
        case 0x002A0000: return "WNNC_NET_STAC";
        case 0x002B0000: return "WNNC_NET_FOXBAT";
        case 0x002C0000: return "WNNC_NET_YAHOO";
        case 0x002D0000: return "WNNC_NET_EXIFS";
        case 0x002E0000: return "WNNC_NET_DAV";
        case 0x002F0000: return "WNNC_NET_KNOWARE";
        case 0x00300000: return "WNNC_NET_OBJECT_DIRE";
        case 0x00310000: return "WNNC_NET_MASFAX";
        case 0x00320000: return "WNNC_NET_HOB_NFS";
        case 0x00330000: return "WNNC_NET_SHIVA";
        case 0x00340000: return "WNNC_NET_IBMAL";
        case 0x00350000: return "WNNC_NET_LOCK";
        case 0x00360000: return "WNNC_NET_TERMSRV";
        case 0x00370000: return "WNNC_NET_SRT";
        case 0x00380000: return "WNNC_NET_QUINCY";
        case 0x00390000: return "WNNC_NET_OPENAFS";
        case 0x003A0000: return "WNNC_NET_AVID1";
        case 0x003B0000: return "WNNC_NET_DFS";
        case 0x003C0000: return "WNNC_NET_KWNP";
        case 0x003D0000: return "WNNC_NET_ZENWORKS";
        case 0x003E0000: return "WNNC_NET_DRIVEONWEB";
        case 0x003F0000: return "WNNC_NET_VMWARE";
        case 0x00400000: return "WNNC_NET_RSFX";
        case 0x00410000: return "WNNC_NET_MFILES";
        case 0x00420000: return "WNNC_NET_MS_NFS";
        case 0x00430000: return "WNNC_NET_GOOGLE";
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
