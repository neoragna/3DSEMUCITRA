// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/apt/apt.h"
#include "core/hle/service/apt/apt_a.h"

namespace Service {
namespace APT {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, GetLockHandle,                "GetLockHandle?"},
    {0x00020080, Initialize,                   "Initialize?"},
    {0x00030040, Enable,                       "Enable?"},
    {0x00040040, nullptr,                      "Finalize?"},
    {0x00050040, GetAppletManInfo,             "GetAppletManInfo"},
    {0x00060040, GetAppletInfo,                "GetAppletInfo"},
    {0x00080000, CountRegisteredApplet,        "CountRegisteredApplet"},
    {0x00090040, IsRegistered,                 "IsRegistered"},
    {0x000B0040, InquireNotification,          "InquireNotification"},
    {0x000C0104, SendParameter,                "SendParameter"},
    {0x000D0080, ReceiveParameter,             "ReceiveParameter"},
    {0x000E0080, GlanceParameter,              "GlanceParameter"},
    {0x000F0100, CancelParameter,              "CancelParameter"},
    {0x00130000, GetPreparationState,          "GetPreparationState"},
    {0x00140040, SetPreparationState,          "SetPreparationState"},
    {0x00150140, PrepareToStartApplication,    "PrepareToStartApplication"},
    {0x00160040, PreloadLibraryApplet,         "PreloadLibraryApplet"},
    {0x00180040, PrepareToStartLibraryApplet,  "PrepareToStartLibraryApplet"},
    {0x001B00C4, StartApplication,             "StartApplication"},
    {0x001E0084, StartLibraryApplet,           "StartLibraryApplet"},
    {0x00220040, PrepareToCloseApplication,    "PrepareToCloseApplication"},
    {0x00270044, CloseApplication,             "CloseApplication"},
    {0x002E0044, LeaveHomeMenu,                "LeaveHomeMenu"},
    {0x002F0040, PrepareToLeaveResidentApplet, "PrepareToLeaveResidentApplet"},
    {0x00300044, LeaveResidentApplet,          "LeaveResidentApplet"},
    {0x00380040, PreloadResidentApplet,        "PreloadResidentApplet"},
    {0x00390040, PrepareToStartResidentApplet, "PrepareToStartResidentApplet"},
    {0x003A0044, StartResidentApplet,          "StartResidentApplet"},
    {0x003B0040, CancelLibraryApplet,          "CancelLibraryApplet?"},
    {0x003E0080, ReplySleepQuery,              "ReplySleepQuery"},
    {0x003F0040, ReplySleepNotificationComplete,"ReplySleepNotificationComplete"},
    {0x00400042, SendCaptureBufferInfo,        "SendCaptureBufferInfo"},
    {0x00410040, ReceiveCaptureBufferInfo,     "ReceiveCaptureBufferInfo"},
    {0x00430040, NotifyToWait,                 "NotifyToWait?"},
    {0x00440000, GetSharedFont,                "GetSharedFont?"},
    {0x00450040, GetWirelessRebootInfo,        "GetWirelessRebootInfo"},
    {0x004A0040, GetCaptureInfo,               "GetCaptureInfo"},
    {0x004B00C2, AppletUtility,                "AppletUtility?"},
    {0x004F0080, SetAppCpuTimeLimit,           "SetAppCpuTimeLimit"},
    {0x00500040, GetAppCpuTimeLimit,           "GetAppCpuTimeLimit"},
    {0x00510080, GetStartupArgument,           "GetStartupArgument"},
    {0x00550040, SetScreenCapPostPermission,   "SetScreenCapPostPermission"},
    {0x00560000, GetScreenCapPostPermission,   "GetScreenCapPostPermission"},
    {0x01010000, CheckNew3DSApp,               "CheckNew3DSApp"},
    {0x01020000, CheckNew3DS,                  "CheckNew3DS"},
    {0x01040000, IsStandardMemoryLayout,       "IsStandardMemoryLayout" },
    {0x01050100, nullptr,                      "IsTitleAllowed" }
};

APT_A_Interface::APT_A_Interface() {
    Register(FunctionTable);
}

} // namespace APT
} // namespace Service
