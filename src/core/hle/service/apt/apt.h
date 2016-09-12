// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/common_types.h"
#include "common/swap.h"

#include "core/hle/kernel/kernel.h"

namespace Service {

class Interface;

namespace APT {

/// Holds information about the parameters used in Send/Glance/ReceiveParameter
struct MessageParameter {
    u32 sender_id = 0;
    u32 destination_id = 0;
    u32 signal = 0;
    Kernel::SharedPtr<Kernel::Object> object = nullptr;
    std::vector<u8> buffer;
};

/// Holds information about the parameters used in StartLibraryApplet
struct AppletStartupParameter {
    Kernel::SharedPtr<Kernel::Object> object = nullptr;
    std::vector<u8> buffer;
};

/// Used by the application to pass information about the current framebuffer to applets.
struct CaptureBufferInfo {
    u32_le size;
    u8 is_3d;
    INSERT_PADDING_BYTES(0x3); // Padding for alignment
    u32_le top_screen_left_offset;
    u32_le top_screen_right_offset;
    u32_le top_screen_format;
    u32_le bottom_screen_left_offset;
    u32_le bottom_screen_right_offset;
    u32_le bottom_screen_format;
};
static_assert(sizeof(CaptureBufferInfo) == 0x20, "CaptureBufferInfo struct has incorrect size");

struct WirelessRebootInfo {
    std::array<u8, 6> mac_address{};
    std::array<u8, 9> wireless_reboot_passphrase{};
    INSERT_PADDING_BYTES(1);
};
static_assert(sizeof(WirelessRebootInfo) == 0x10, "WirelessRebootInfo struct has incorrect size");

/// Signals used by APT functions
enum class SignalType : u32 {
    None              = 0x0,
    AppJustStarted    = 0x1,
    LibAppJustStarted = 0x2,
    LibAppFinished    = 0x3,
    LibAppClosed      = 0xA,
    ReturningToApp    = 0xB,
    ExitingApp        = 0xC,
};

/// App Id's used by APT functions
enum class AppletId : u32 {
    HomeMenu           = 0x101,
    AlternateMenu      = 0x103,
    Camera             = 0x110,
    FriendsList        = 0x112,
    GameNotes          = 0x113,
    InternetBrowser    = 0x114,
    InstructionManual  = 0x115,
    Notifications      = 0x116,
    Miiverse           = 0x117,
    MiiversePost       = 0x118,
    AmiiboSettings     = 0x119,
    SoftwareKeyboard1  = 0x201,
    Ed1                = 0x202,
    PnoteApp1          = 0x204,
    SnoteApp1          = 0x205,
    Error              = 0x206,
    Mint1              = 0x207,
    Extrapad1          = 0x208,
    Memolib1           = 0x209,
    Application        = 0x300,
    AnyLibraryApplet   = 0x400,
    SoftwareKeyboard2  = 0x401,
    Ed2                = 0x402,
    PnoteApp2          = 0x404,
    SnoteApp2          = 0x405,
    Error2             = 0x406,
    Mint2              = 0x407,
    Extrapad2          = 0x408,
    Memolib2           = 0x409,
};

enum class AppletPos {
    None     = -1,
    App      = 0,
    AppLib   = 1,
    Sys      = 2,
    SysLib   = 3,
    Resident = 4
};

enum class StartupArgumentType : u32 {
    OtherApp   = 0,
    Restart    = 1,
    OtherMedia = 2,
};

enum class ScreencapPostPermission : u32 {
    CleanThePermission                 = 0, //TODO(JamePeng): verify what "zero" means
    NoExplicitSetting                  = 1,
    EnableScreenshotPostingToMiiverse  = 2,
    DisableScreenshotPostingToMiiverse = 3
};

enum class QueryReply : u32 {
    Reject = 0,
    Accept = 1,
    Later  = 2
};

enum class AppletPreparationState : u32 {
    NoPreparation               = 0,
    PreparedToLaunchApp         = 1,
    PreparedToCloseApp          = 2,
    PreparedToForceToCloseApp   = 3,
    PreparedToPreloadApplib     = 4,
    PreparedToLaunchApplib      = 5,
    PreparedToCloseApplib       = 6,
    PreparedToLaunchSys         = 7,
    PreparedToCloseSys          = 8,
    PreparedToPreloadSyslib     = 9,
    PreparedToLaunchSyslib      = 10,
    PreparedToCloseSyslib       = 11,
    PreparedToLaunchResident    = 12,
    PreparedToLeaveResident     = 13,
    PreparedToDoHomemenu        = 14,
    PreparedToLeaveHomemenu     = 15,
    PreparedToStartResident     = 16,
    PreparedToDoAppJump         = 17,
    PreparedToForceToCloseSys   = 18,
    PreparedToLaunchOtherSys    = 19,
    PreparedToJumpToApp         = 20,
};

enum class UtilityID : u32 {
    ClearPowerButtonState  = 0,
    ClearExclusiveControl  = 3,
    SleepIfShellClosed     = 4,
    LockTransition         = 5,
    TryLockTransition      = 6,
    UnlockTransition       = 7,
    StartExitTask          = 10,
    SetInitialSenderId     = 11,
    SetPowerButtonClick    = 12,
    SetHomeMenuBuffer      = 17
};

/// Send a parameter to the currently-running application, which will read it via ReceiveParameter
void SendParameter(const MessageParameter& parameter);

/**
 * APT::Initialize service function
 * Service function that initializes the APT process for the running application
 *  Outputs:
 *      1 : Result of the function, 0 on success, otherwise error code
 *      3 : Handle to the notification event
 *      4 : Handle to the pause event
 */
void Initialize(Service::Interface* self);

/**
 * APT::GetSharedFont service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Virtual address of where shared font will be loaded in memory
 *      4 : Handle to shared font memory
 */
void GetSharedFont(Service::Interface* self);

/**
 * APT::GetWirelessRebootInfo service function
 *  Inputs:
 *      1 : buffer_size
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *  Note:
 *      Same as NSS:SetWirelessRebootInfo except this loads the data instead.
 *      The state flag set by NSS:SetWirelessRebootInfo must have bit0 set,
 *      Otherwise the output buffer is just cleared without copying any data.
 *      This is used by DLP-child titles.
 */
void GetWirelessRebootInfo(Service::Interface* self);

/**
 * APT::PrepareToLeaveResidentApplet service function
 *  Inputs:
 *      1 : u8, Caller Exiting (0 = not exiting, 1 = exiting)
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void PrepareToLeaveResidentApplet(Service::Interface* self);

/**
 * APT::LeaveResidentApplet service function
 *  Inputs:
 *      1 : Parameters Size
 *      2 : 0x0
 *      3 : Handle Parameter
 *      5 : void*, Parameters
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void LeaveResidentApplet(Service::Interface* self);

/**
 * APT::PreloadResidentApplet service function
 *  Inputs:
 *      1 : AppID
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void PreloadResidentApplet(Service::Interface* self);

/**
 * APT::PrepareToStartResidentApplet service function
 *  Inputs:
 *      1 : AppID
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void PrepareToStartResidentApplet(Service::Interface* self);

/**
 * APT::StartResidentApplet service function
 *  Inputs:
 *      1 : Parameters Size
 *      2 : 0x0
 *      3 : Handle Parameter
 *      5 : void*, Parameters
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void StartResidentApplet(Service::Interface* self);

/**
 * APT::CancelLibraryApplet service function
 *  Inputs:
 *      0 : Header code [0x003B0040]
 *      1 : u8, Application Exiting (0 = not exiting, 1 = exiting)
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void CancelLibraryApplet(Service::Interface* self);

/**
 * APT::ReplySleepQuery service function
 *  Inputs:
 *      1 : AppID
 *      2:  QueryReply
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *  Note: If the specific unimplemented function or applet was called by application,
 *        It would return a value QueryReply::Reject to cmd_buff[2]
 *        For example, "Could not create applet XXXX" will cause the command QueryReply::Reject to this function
 */
void ReplySleepQuery(Service::Interface* self);

/**
 * APT::ReplySleepNotificationComplete service function
 *  Inputs:
 *      1 : AppId
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void ReplySleepNotificationComplete(Service::Interface* self);

/**
 * APT::SendCaptureBufferInfo service function
 *  Inputs:
 *      1 : Size
 *      2 : (Size << 14) | 2
 *      3:  void* CaptureBufferInfo
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *  Note:
 *      The input size is clamped to 0x20-bytes by NS.
 *      The input buffer with the clamped size is then copied to a NS state buffer.
 *      The size field for this state buffer is also set to this clamped size.
 */
void SendCaptureBufferInfo(Service::Interface* self);

/**
 * APT::ReceiveCaptureBufferInfo service function
 *  Inputs:
 *      1 : Size
 *   0x40 : (Size << 14) | 2
 *   0x41 : void*, CaptureBufferInfo
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Actual Size
 *  Note:
 *      This command loads the data set by APT:SendCaptureBufferInfo.
 *      The input size is clamped to 0x20-bytes by NS, then this size is clamped to the size for the NS state buffer.
 *      The NS state buffer data is copied to the output buffer, (when the clamped size is not zero) then the size field for the state buffer is set 0.
 */
void ReceiveCaptureBufferInfo(Service::Interface* self);

/**
 * APT::GetCaptureInfo service function
 *  Inputs:
 *      1 : Size
 *   0x40 : (Size << 14) | 2
 *   0x41 : void*, CaptureBufferInfo
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *  Notes:
 *      This command loads the data set by APT:SendCaptureBufferInfo, this command is similar to APT:ReceiveCaptureBufferInfo.
 *      The input size is clamped to 0x20-bytes by NS.
 *      The NS state buffer data is then copied to the output buffer.
 */
void GetCaptureInfo(Service::Interface* self);

/**
 * APT::NotifyToWait service function
 *  Inputs:
 *      1 : AppID
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void NotifyToWait(Service::Interface* self);

/**
 * APT::GetLockHandle service function
 *  Inputs:
 *      1 : Applet attributes
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Applet attributes
 *      3 : Power button state
 *      4 : IPC handle descriptor
 *      5 : APT mutex handle
 */
void GetLockHandle(Service::Interface* self);

/**
 * APT::Enable service function
 *  Inputs:
 *      1 : Applet attributes
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void Enable(Service::Interface* self);

/**
 * APT::GetAppletManInfo service function.
 *  Inputs:
 *      1 : AppletPos
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : AppletPos
 *      3 : Requested AppId
 *      4 : Home Menu AppId
 *      5 : AppID of currently active app
 */
void GetAppletManInfo(Service::Interface* self);

/**
 * APT::GetAppletInfo service function.
 *  Inputs:
 *      1 : AppId
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2-3 : Title ID
 *      4 : Media Type
 *      5 : Registered
 *      6 : Loaded
 *      7 : Attributes
 */
void GetAppletInfo(Service::Interface* self);

/**
 * APT::CountRegisteredApplet service function.
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Registered Applet Count
 */
void CountRegisteredApplet(Service::Interface* self);

/**
 * APT::IsRegistered service function
 *  Inputs:
 *      1 : AppID
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Output, 0 = not registered, 1 = registered.
 *  Note:
 *      This returns whether the specified AppID is registered with NS yet.
 *      An AppID is "registered" once the process associated with the AppID uses APT:Enable. Home Menu uses this
 *      command to determine when the launched process is running and to determine when to stop using GSP etc,
 *      while displaying the "Nintendo 3DS" loading screen.
 */
void IsRegistered(Service::Interface* self);

/**
 * APT::InquireNotification service function.
 *  Inputs:
 *      1 : AppId
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Notification
 */
void InquireNotification(Service::Interface* self);

/**
 * APT::SendParameter service function. This sets the parameter data state.
 * Inputs:
 *     1 : Source AppID
 *     2 : Destination AppID
 *     3 : Signal type
 *     4 : Parameter buffer size, max size is 0x1000 (this can be zero)
 *     5 : Value
 *     6 : Handle to the destination process, likely used for shared memory (this can be zero)
 *     7 : (Size<<14) | 2
 *     8 : Input parameter buffer ptr
 * Outputs:
 *     0 : Return Header
 *     1 : Result of function, 0 on success, otherwise error code
*/
void SendParameter(Service::Interface* self);

/**
 * APT::ReceiveParameter service function
 *  Inputs:
 *      1 : AppID
 *      2 : Parameter buffer size, max size is 0x1000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : AppID of the process which sent these parameters
 *      3 : Signal type
 *      4 : Actual parameter buffer size, this is <= to the the input size
 *      5 : Value
 *      6 : Handle from the source process which set the parameters, likely used for shared memory
 *      7 : Size
 *      8 : Output parameter buffer ptr
 *   Note:
 *        This returns the current parameter data from NS state,
 *        from the source process which set the parameters. Once finished, NS will clear a flag in the NS
 *        state so that this command will return an error if this command is used again if parameters were
 *        not set again. This is called when the second Initialize event is triggered. It returns a signal
 *        type indicating why it was triggered.
 */
void ReceiveParameter(Service::Interface* self);

/**
 * APT::GlanceParameter service function
 *  Inputs:
 *      1 : AppID
 *      2 : Parameter buffer size, max size is 0x1000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Sender AppID, for now assume AppID of the process which sent these parameters
 *      3 : Command, for now assume Signal type
 *      4 : Actual parameter buffer size, this is <= to the the input size
 *      5 : Value
 *      6 : Handle from the source process which set the parameters, likely used for shared memory
 *      7 : Size
 *      8 : Output parameter buffer ptr
 *   Note:
 *      This is exactly the same as APT_U::ReceiveParameter
 *      (except for the word value prior to the output handle), except this will not clear the flag
 *      (except when responseword[3]==8 || responseword[3]==9) in NS state.
 */
void GlanceParameter(Service::Interface* self);

/**
 * APT::CancelParameter service function.
 *  Inputs:
 *      1 : u8, Check Sender (0 = don't check, 1 = check)
 *      2 : Sender AppID
 *      3 : u8, Check Receiver (0 = don't check, 1 = check)
 *      4 : Receiver AppID
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Status flag, 0 = failure due to no parameter data being available, or the above enabled
 *          fields don't match the fields in NS state. 1 = success.
 *  Note:
 *      When the parameter data is available, and when the above specified fields match the ones in NS state(for the ones where the checks are enabled),
 *      This clears the flag which indicates that parameter data is available(same flag cleared by APT:ReceiveParameter).
 */
void CancelParameter(Service::Interface* self);

/**
 * APT::GetPreparationState service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2:  AppletPreparationState
 */
void GetPreparationState(Service::Interface* self);

/**
 * APT::SetPreparationState service function
 *  Inputs:
 *      1 : AppletPreparationState
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void SetPreparationState(Service::Interface* self);

/**
 * APT::PrepareToStartApplication service function
 *  Inputs:
 *    1-4 : ProgramInfo
 *      5 : Flags (usually zero, when zero, NS writes a title-info struct with Program ID = ~0
 *                 and MediaType = NAND to the FIRM parameters structure)
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *  Note:
 *      When the input title-info programID is zero,
 *      NS will load the actual program ID via AMNet:GetTitleIDList. After doing some checks with the
 *      programID, NS will then set a NS state flag to value 1, then set the programID for AppID
 *      0x300(application) to the input program ID(or the one from GetTitleIDList). A media-type field
 *      in the NS state is also set to the input media-type value
 *      (other state fields are set at this point as well). With 8.0.0-18, NS will set an u8 NS state
 *      field to value 1 when input flags bit8 is set
 */
void PrepareToStartApplication(Service::Interface* self);

/**
 * APT::StartApplication service function.
 * Inputs:
 *     1 : Parameter Size (capped to 0x300)
 *     2 : HMAC Size (capped to 0x20)
 *     3 : u8 Paused (0 = not paused, 1 = paused)
 *     4 : (Parameter Size << 14) | 2
 *     5 : void*, Parameter
 *     6 : (HMAC Size << 14) | 0x802
 *     7 : void*, HMAC
 * Outputs:
 *     0 : Return Header
 *     1 : Result of function, 0 on success, otherwise error code
 * Note:
 *     The parameter buffer is copied to NS FIRMparams+0x0, then the HMAC buffer is copied to NS FIRMparams+0x480.
 *     Then the application is launched.
 */
void StartApplication(Service::Interface* self);

/**
 * APT::AppletUtility service function
 *  Inputs:
 *      1 : UtilityID
 *      2 : Input Size
 *      3 : Output Size
 *      4 : (Input Size << 14) | 0x402
 *      5 : void*, Input
 *   0x41 : void*, Output
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Applet Result
 *  Note:
 *      This function may affect specific srv:Notifications operation by using Utility
 */
void AppletUtility(Service::Interface* self);

/**
 * APT::SetAppCpuTimeLimit service function
 *  Inputs:
 *      1 : Value, must be one
 *      2 : Percentage of CPU time from 5 to 80
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void SetAppCpuTimeLimit(Service::Interface* self);

/**
 * APT::GetAppCpuTimeLimit service function
 *  Inputs:
 *      1 : Value, must be one
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : System core CPU time percentage
 */
void GetAppCpuTimeLimit(Service::Interface* self);

/**
 * APT::PrepareToStartLibraryApplet service function
 *  Inputs:
 *      0 : Command header [0x00180040]
 *      1 : Id of the applet to start
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
void PrepareToStartLibraryApplet(Service::Interface* self);

/**
 * APT::PreloadLibraryApplet service function
 *  Inputs:
 *      0 : Command header [0x00160040]
 *      1 : Id of the applet to start
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
void PreloadLibraryApplet(Service::Interface* self);

/**
 * APT::StartLibraryApplet service function
 *  Inputs:
 *      0 : Command header [0x001E0084]
 *      1 : Id of the applet to start
 *      2 : Buffer size
 *      3 : Always 0?
 *      4 : Handle passed to the applet
 *      5 : (Size << 14) | 2
 *      6 : Input buffer virtual address
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
void StartLibraryApplet(Service::Interface* self);

/**
 * APT::PrepareToCloseApplication service function
 *  Inputs:
 *      1 : u8, Cancel Preload (0 = don't cancel, 1 = cancel)
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void PrepareToCloseApplication(Service::Interface* self);

/**
 * APT::CloseApplication service function
 *  Inputs:
 *      1 : Parameters Size
 *      2 : 0x0
 *      3 : Handle Parameter
 *      4 : (Parameters Size << 14) | 2
 *      5 : void*, Parameters
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void CloseApplication(Service::Interface* self);

/**
 * APT::LeaveHomeMenu service function
 *  Inputs:
 *      0 : Header code [0x002E0044]
 *      1 : Parameters Size
 *      2 : 0x0
 *      3 : Handle Parameter
 *      4 : (Parameters Size << 14) | 2
 *      5 : void*, Parameters
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void LeaveHomeMenu(Service::Interface* self);

/**
 * APT::GetStartupArgument service function
 *  Inputs:
 *      1 : Parameter Size (capped to 0x300)
 *      2 : StartupArgumentType
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8, Exists (0 = does not exist, 1 = exists)
 */
void GetStartupArgument(Service::Interface* self);

/**
 * APT::SetScreenCapPostPermission service function
 *  Inputs:
 *      0 : Header Code[0x00550040]
 *      1 : u8 The screenshot posting permission
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void SetScreenCapPostPermission(Service::Interface* self);

/**
 * APT::GetScreenCapPostPermission service function
 *  Inputs:
 *      0 : Header Code[0x00560000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8 The screenshot posting permission
 */
void GetScreenCapPostPermission(Service::Interface* self);

/**
 * APT::CheckNew3DSApp service function
 *  Outputs:
 *      1: Result code, 0 on success, otherwise error code
 *      2: u8 output: 0 = Old3DS, 1 = New3DS.
 *  Note:
 *  This uses PTMSYSM:CheckNew3DS.
 *  When a certain NS state field is non-zero, the output value is zero,
 *  Otherwise the output is from PTMSYSM:CheckNew3DS.
 *  Normally this NS state field is zero, however this state field is set to 1
 *  when APT:PrepareToStartApplication is used with flags bit8 is set.
 */
void CheckNew3DSApp(Service::Interface* self);

/**
 * Wrapper for PTMSYSM:CheckNew3DS
 * APT::CheckNew3DS service function
 *  Outputs:
 *      1: Result code, 0 on success, otherwise error code
 *      2: u8 output: 0 = Old3DS, 1 = New3DS.
 */
void CheckNew3DS(Service::Interface* self);

/**
 * APT::IsStandardMemoryLayout service function
 * Inputs:
 *      0 : Header code [0x01040000]
 * Outputs:
 *      0 : Header code
 *      1 : Result code
 *      2 : u8, Standard Memory Layout (0 = non-standard, 1 = standard)
 */
void IsStandardMemoryLayout(Service::Interface* self);

/// Initialize the APT service
void Init();

/// Shutdown the APT service
void Shutdown();

} // namespace APT
} // namespace Service
