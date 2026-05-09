#include "HtmlWriter.h"
#include <fstream>
#include <filesystem>

namespace lecmd {

static const char* kNormalizeCss = R"(/*! normalize.css v3.0.3 | MIT License | github.com/necolas/normalize.css */
html{font-family:sans-serif;-ms-text-size-adjust:100%;-webkit-text-size-adjust:100%}
body{margin:0}
article,aside,details,figcaption,figure,footer,header,main,menu,nav,section,summary{display:block}
audio,canvas,progress,video{display:inline-block;vertical-align:baseline}
a{background-color:transparent}
a:active,a:hover{outline:0}
b,strong{font-weight:bolder}
dfn{font-style:italic}
h1{font-size:2em;margin:0.67em 0}
mark{background:#ff0;color:#000}
small{font-size:80%}
sub,sup{font-size:75%;line-height:0;position:relative;vertical-align:baseline}
sup{top:-0.5em}
sub{bottom:-0.25em}
img{border:0}
svg:not(:root){overflow:hidden}
figure{margin:1em 40px}
hr{box-sizing:content-box;height:0;overflow:visible}
pre{overflow:auto}
code,kbd,pre,samp{font-family:monospace,monospace;font-size:1em}
button,input,optgroup,select,textarea{font:inherit;margin:0}
button{overflow:visible}
button,select{text-transform:none}
button,html input[type=button],input[type=reset],input[type=submit]{-webkit-appearance:button;cursor:pointer}
button[disabled],html input[disabled]{cursor:default}
button::-moz-focus-inner,input::-moz-focus-inner{border:0;padding:0}
button:-moz-focusring,input:-moz-focusring{outline:1px dotted ButtonText}
input{line-height:normal}
input[type=checkbox],input[type=radio]{box-sizing:border-box;padding:0}
input[type=number]::-webkit-inner-spin-button,input[type=number]::-webkit-outer-spin-button{height:auto}
input[type=search]{-webkit-appearance:textfield}
input[type=search]::-webkit-search-cancel-button,input[type=search]::-webkit-search-decoration{-webkit-appearance:none}
fieldset{border:1px solid silver;margin:0 2px;padding:.35em .625em .75em}
legend{border:0;padding:0}
textarea{overflow:auto}
optgroup{font-weight:700}
)";

static const char* kStyleCss = R"CSS(Wrapper_Forensicator{width:1400px;height:auto;border:medium #146B99 solid;resize:both;overflow:auto;min-width:50px;min-height:50px;display:block;padding:0;margin:10px;background-color:#D9ECFF}
Forensicator{letter-spacing:1pt;line-height:1;word-spacing:2pt;font-size:14px;font-family:verdana,sans-serif;text-align:right;padding:2px 15px 0 2px;margin:2px 0 4px 0}
Container{width:1400px;height:auto;border:medium #146B99 solid;resize:both;overflow:auto;min-width:50px;min-height:50px;display:block;padding:0;margin:10px;background-color:#E9F0F4}
SourceFile{display:block;width:auto;font-weight:bold;font-size:16px;background-color:#146B99;color:#FFBD6F;padding:0;margin:0}
SourceCreated,SourceModified,SourceAccessed,TargetCreated,TargetModified,TargetAccessed,FileSize,RelativePath,WorkingDirectory,FileAttributes,HeaderFlags,DriveType,VolumeSerialNumber,VolumeLabel,LocalPath,CommonPath,NetworkPath,Arguments,TargetIDAbsolutePath,TargetMFTEntryNumber,TargetMFTSequenceNumber,MachineID,MachineMACAddress,MACVendor,TrackerCreatedOn,ExtraBlocksPresent{display:block;width:auto;color:#043F5F;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:normal;font-size:12px;font-family:verdana,sans-serif;text-align:left;padding:2px 0 0 30px;margin:4px 0 4px 0}
SourceCreated:before{content:"Source Created:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 20px 0 2px;margin:2px 0 4px 0}
SourceModified:before{content:"Source Modified:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 15px 0 2px;margin:2px 0 4px 0}
SourceAccessed:before{content:"Source Accessed:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
TargetCreated:before{content:"Target Created:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 23px 0 2px;margin:2px 0 4px 0}
TargetModified:before{content:"Target Modified:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 18px 0 2px;margin:2px 0 4px 0}
TargetAccessed:before{content:"Target Accessed:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
FileSize:before{content:"File Size:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
FileSize:after{content:" [bytes]";color:#A6A8A9}
RelativePath:before{content:"Relative Path:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
WorkingDirectory:before{content:"Working Directory:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
FileAttributes:before{content:"File Attributes:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
HeaderFlags:before{content:"Header Flags:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
DriveType:before{content:"Drive Type:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
VolumeSerialNumber:before{content:"Volume Serial Number:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
VolumeLabel:before{content:"Volume Label:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
LocalPath:before{content:"Local Path:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
CommonPath:before{content:"Common Path:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
NetworkPath:before{content:"Network Path:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
Arguments:before{content:"Arguments:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
TargetIDAbsolutePath:before{content:"TargetID Absolute Path:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
TargetMFTEntryNumber{color:maroon}
TargetMFTEntryNumber:before{content:"Target $MFT Entry Number:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
TargetMFTSequenceNumber{color:maroon}
TargetMFTSequenceNumber:before{content:"Target $MFT Sequence Number:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
MachineID:before{content:"MachineID:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
MachineMACAddress:before{content:"Machine MAC Address:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
MACVendor:before{content:"MAC Vendor:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
MACVendor:after{content:" [vendor not included in source .lnk file, auto-resolved by LECmd for end-user upon parsing]";color:#A6A8A9}
TrackerCreatedOn:before{content:"Tracker Created On:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
ExtraBlocksPresent:before{content:"Extra Blocks Present:";display:inline;letter-spacing:1pt;line-height:1;word-spacing:2pt;font-weight:bold;font-size:12px;font-family:verdana,sans-serif;text-align:right;text-decoration:underline;padding:2px 10px 0 2px;margin:2px 0 4px 0}
)CSS";

static std::string XhtmlEscape(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '&': r += "&amp;"; break;
            case '<': r += "&lt;"; break;
            case '>': r += "&gt;"; break;
            case '"': r += "&quot;"; break;
            default: r += c; break;
        }
    }
    return r;
}

static void WriteElement(std::ofstream& file, const std::string& name, const std::string& value) {
    file << "    <" << name << ">" << XhtmlEscape(value) << "</" << name << ">\n";
}

bool HtmlWriter::Write(const std::string& path, const std::vector<CsvOut>& entries) {
    // path is the html directory. We need to create a subdirectory for this run.
    // But the caller in main.cpp now handles the subdirectory creation.
    // So 'path' should be the full path to index.xhtml.
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<?xml-stylesheet href=\"normalize.css\"?>\n";
    file << "<?xml-stylesheet href=\"style.css\"?>\n";
    file << "<document>\n";

    for (const auto& e : entries) {
        file << "  <Container>\n";
        WriteElement(file, "SourceFile", e.sourceFile);
        WriteElement(file, "SourceCreated", e.sourceCreated);
        WriteElement(file, "SourceModified", e.sourceModified);
        WriteElement(file, "SourceAccessed", e.sourceAccessed);
        WriteElement(file, "TargetCreated", e.targetCreated);
        WriteElement(file, "TargetModified", e.targetModified);
        WriteElement(file, "TargetAccessed", e.targetAccessed);
        file << "    <FileSize>" << e.fileSize << "</FileSize>\n";
        WriteElement(file, "RelativePath", e.relativePath);
        WriteElement(file, "WorkingDirectory", e.workingDirectory);
        WriteElement(file, "FileAttributes", e.fileAttributes);
        WriteElement(file, "HeaderFlags", e.headerFlags);
        WriteElement(file, "DriveType", e.driveType);
        WriteElement(file, "VolumeSerialNumber", e.volumeSerialNumber);
        WriteElement(file, "VolumeLabel", e.volumeLabel);
        WriteElement(file, "LocalPath", e.localPath);
        WriteElement(file, "NetworkPath", e.networkPath);
        WriteElement(file, "CommonPath", e.commonPath);
        WriteElement(file, "Arguments", e.arguments);
        WriteElement(file, "TargetIDAbsolutePath", e.targetIDAbsolutePath);
        WriteElement(file, "TargetMFTEntryNumber", e.targetMFTEntryNumber);
        WriteElement(file, "TargetMFTSequenceNumber", e.targetMFTSequenceNumber);
        WriteElement(file, "MachineID", e.machineID);
        WriteElement(file, "MachineMACAddress", e.machineMACAddress);
        WriteElement(file, "MACVendor", e.macVendor);
        WriteElement(file, "TrackerCreatedOn", e.trackerCreatedOn);
        WriteElement(file, "ExtraBlocksPresent", e.extraBlocksPresent);
        file << "  </Container>\n";
    }

    file << "</document>\n";
    return true;
}

const char* HtmlWriter::GetNormalizeCss() {
    return kNormalizeCss;
}

const char* HtmlWriter::GetStyleCss() {
    return kStyleCss;
}

} // namespace lecmd
