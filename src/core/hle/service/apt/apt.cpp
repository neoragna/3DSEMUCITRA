// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"

#include "core/hle/applets/applet.h"
#include "core/hle/config_mem.h"
#include "core/hle/service/service.h"
#include "core/hle/service/apt/apt.h"
#include "core/hle/service/apt/apt_a.h"
#include "core/hle/service/apt/apt_s.h"
#include "core/hle/service/apt/apt_u.h"
#include "core/hle/service/apt/bcfnt/bcfnt.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/gsp_gpu.h"
#include "core/hle/service/ptm/ptm.h"

#include "core/hle/kernel/event.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/shared_memory.h"

#include "core/hw/gpu.h"
#include "core/settings.h"

namespace Service {
namespace APT {

/// Handle to shared memory region designated to for shared system font
static Kernel::SharedPtr<Kernel::SharedMemory> shared_font_mem;
static bool shared_font_relocated = false;

static Kernel::SharedPtr<Kernel::Mutex> lock;
static Kernel::SharedPtr<Kernel::Event> notification_event; ///< APT notification event
static Kernel::SharedPtr<Kernel::Event> parameter_event; ///< APT parameter event

static u32 cpu_percent; ///< CPU time available to the running application
static u32 homemenu_input_buffer;
static bool is_capture_buffer_info_set = false;
static bool is_homemenu_input_buffer_set = false;

static AppletPreparationState applet_preparation_state = AppletPreparationState::NoPreparation;
static CaptureBufferInfo capture_buffer_info;
static WirelessRebootInfo wireless_reboot_info;

// APT::CheckNew3DSApp will check this unknown_ns_state_field to determine processing mode
static u8 unknown_ns_state_field;

static ScreencapPostPermission screen_capture_post_permission;

/// Parameter data to be returned in the next call to Glance/ReceiveParameter
static MessageParameter next_parameter;
static std::array<MessageParameter, 0x4> MessageParameters;

/**
 * GetMessageParametersIndex(sub_10F564)
 *   Index 0 :
 *         0x300 : Application
 *   Index 1 :
 *         0x11X : System Applet
 *   Index 2 :
 *         0x10X : Home Menu
 *   Index 3 :
 *         0x2XX : Library Applet
 *         0x4XX : Library Applet
 */
static u32 GetMessageParametersIndex(const u32& app_id) {
    u32 result = 0;
    if (!(app_id & 0xFF)) {
        return 0xFFFFFFFF;
    } else {
        if (app_id == 0x101 || app_id == 0x103) {
            if (app_id & 0x100) {
                return 0x2;
            }
            return 0xFFFFFFFF;
        }
        if ((app_id & 0x300) != app_id) {
            if ((app_id & 0x110) != app_id) {
                if ((app_id & 0x100) != app_id) {
                    if ((app_id & 0x200) != app_id && (app_id & 0x400) != app_id) {
                        return 0xFFFFFFFF;
                    }
                    return 3;
                }
                return 2;
            }
            return 1;
        }
        return 0;
    }
}

void SendParameter(const MessageParameter& parameter) {
    next_parameter = parameter;
    // Signal the event to let the application know that a new parameter is ready to be read
    parameter_event->Signal();
}


void SendParameter(const u32& dst_app_id, const MessageParameter& parameter) {
    u32 index = GetMessageParametersIndex(dst_app_id);
    if(index != 0xFFFFFFFF){
        memcpy(&MessageParameters[index], &parameter, sizeof(MessageParameter));
    }
    // Signal the event to let the application know that a new parameter is ready to be read
    parameter_event->Signal();
}

void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    u32 flags  = cmd_buff[2];

    cmd_buff[2] = IPC::CopyHandleDesc(2);
    cmd_buff[3] = Kernel::g_handle_table.Create(notification_event).MoveFrom();
    cmd_buff[4] = Kernel::g_handle_table.Create(parameter_event).MoveFrom();

    // TODO(bunnei): Check if these events are cleared every time Initialize is called.
    notification_event->Clear();
    parameter_event->Clear();

    ASSERT_MSG((nullptr != lock), "Cannot initialize without lock");
    lock->Release();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_DEBUG(Service_APT, "called app_id=0x%08X, flags=0x%08X", app_id, flags);
}

void GetSharedFont(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    if (!shared_font_mem) {
        LOG_ERROR(Service_APT, "shared font file missing - go dump it from your 3ds");
        cmd_buff[0] = IPC::MakeHeader(0x44, 2, 2);
        cmd_buff[1] = -1;  // TODO: Find the right error code
        return;
    }

    // The shared font has to be relocated to the new address before being passed to the application.
    VAddr target_address = Memory::PhysicalToVirtualAddress(shared_font_mem->linear_heap_phys_address);
    if (!shared_font_relocated) {
        BCFNT::RelocateSharedFont(shared_font_mem, target_address);
        shared_font_relocated = true;
    }
    cmd_buff[0] = IPC::MakeHeader(0x44, 2, 2);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    // Since the SharedMemory interface doesn't provide the address at which the memory was allocated,
    // the real APT service calculates this address by scanning the entire address space (using svcQueryMemory)
    // and searches for an allocation of the same size as the Shared Font.
    cmd_buff[2] = target_address;
    cmd_buff[3] = IPC::CopyHandleDesc();
    cmd_buff[4] = Kernel::g_handle_table.Create(shared_font_mem).MoveFrom();
}

void GetWirelessRebootInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 buffer_size = cmd_buff[1];
    VAddr buffer_addr = cmd_buff[0x41];

    if (buffer_size > sizeof(WirelessRebootInfo)) {
        buffer_size = sizeof(WirelessRebootInfo);
    }

    memcpy(&wireless_reboot_info, Memory::GetPointer(buffer_addr), sizeof(buffer_size));

    cmd_buff[0] = IPC::MakeHeader(0x45, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buffer_size << 14) | 2;
    cmd_buff[3] = buffer_size;
    LOG_WARNING(Service_APT, "(STUBBED) called");
}

void PrepareToLeaveResidentApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 caller_exiting = cmd_buff[1] & 0xFF;

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_APT, "(STUBBED) caller_exiting=0x%08X", caller_exiting);
}

void LeaveResidentApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 parameters_size = cmd_buff[1];
    u32 parameter_handle = cmd_buff[3];
    u32 parameters_addr = cmd_buff[5];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_APT, "(STUBBED) parameter_size=0x%08X, parameter_handle=0x%08X, parameter_addr=0x%08X",
        parameters_size, parameter_handle, parameters_addr);
}

void PreloadResidentApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_APT, "(STUBBED) app_id=0x%08X", app_id);
}

void PrepareToStartResidentApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_APT, "(STUBBED) app_id=0x%08X", app_id);
}

void StartResidentApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 parameters_size = cmd_buff[1];
    u32 parameter_handle = cmd_buff[3];
    u32 parameters_addr = cmd_buff[5];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_APT, "(STUBBED) parameter_size=0x%08X, parameter_handle=0x%08X, parameter_addr=0x%08X",
                parameters_size, parameter_handle, parameters_addr);
}

void CancelLibraryApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 application_exiting = cmd_buff[1] & 0xf;

    cmd_buff[0] = IPC::MakeHeader(0x3B, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_APT, "(STUBBED) application_exiting=%u", application_exiting);
}

void ReplySleepQuery(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    QueryReply query_reply = static_cast<QueryReply>(cmd_buff[2] & 0xFF);

    cmd_buff[1] = RESULT_SUCCESS.raw;
    if (query_reply == QueryReply::Reject) {
        LOG_ERROR(Service_APT, "(STUBBED) app_id=0x%08X, query_reply=REJECT", app_id);
    } else {
        LOG_WARNING(Service_APT, "(STUBBED) app_id=0x%08X, query_reply=%u", app_id, query_reply);
    }
}

void ReplySleepNotificationComplete(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_APT, "(STUBBED) app_id=0x%08X", app_id);
}

static void InitCaptureBufferInfo() {
    u32 thread_id = 0;
    u32 top_screen_color_per_pixel;
    u32 bottom_screen_color_per_pixel;

    GSP_GPU::FrameBufferUpdate* top_screen = GSP_GPU::GetFrameBufferInfo(thread_id, 0);
    GSP_GPU::FrameBufferUpdate* bottom_screen = GSP_GPU::GetFrameBufferInfo(thread_id, 1);
    // not 3D
    capture_buffer_info.is_3d = 0;
    // main screen config
    capture_buffer_info.top_screen_left_offset = top_screen->framebuffer_info[top_screen->index].address_left;
    capture_buffer_info.top_screen_right_offset = top_screen->framebuffer_info[top_screen->index].address_right;
    capture_buffer_info.top_screen_format = top_screen->framebuffer_info[top_screen->index].format & 0x7;
    // sub screen config
    capture_buffer_info.bottom_screen_left_offset = bottom_screen->framebuffer_info[bottom_screen->index].address_left;
    capture_buffer_info.bottom_screen_right_offset = bottom_screen->framebuffer_info[bottom_screen->index].address_right;
    capture_buffer_info.bottom_screen_format = bottom_screen->framebuffer_info[bottom_screen->index].format & 0x7;

    top_screen_color_per_pixel = GPU::Regs::BytesPerPixel(static_cast<GPU::Regs::PixelFormat>(capture_buffer_info.top_screen_format));
    bottom_screen_color_per_pixel = GPU::Regs::BytesPerPixel(static_cast<GPU::Regs::PixelFormat>(capture_buffer_info.bottom_screen_format));
    capture_buffer_info.size = 240 * (top_screen_color_per_pixel * 400 + bottom_screen_color_per_pixel * 320);

    is_capture_buffer_info_set = true;
}

ResultCode SendCaptureBufferInfo(u32& buffer_addr, u32& buffer_size) {
    if (buffer_addr) {
        if (buffer_size > sizeof(CaptureBufferInfo)) {
            buffer_size = sizeof(CaptureBufferInfo);
        }
        is_capture_buffer_info_set = true;
    } else {
        buffer_size = 0;
    }

    memcpy(&capture_buffer_info, Memory::GetPointer(buffer_addr), buffer_size);
    return RESULT_SUCCESS;
}

void SendCaptureBufferInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 buffer_size = cmd_buff[1];
    u32 buffer_transition = cmd_buff[2];
    u32 buffer_addr = cmd_buff[3];

    if ((buffer_transition & 0x3C0F) != 0x2) {
        cmd_buff[0] = IPC::MakeHeader(0, 0x1, 0);
        cmd_buff[1] = ResultCode(ErrorDescription::OS_InvalidBufferDescriptor, ErrorModule::OS,
                                 ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw; //0xD9001830
        return;
    }

    cmd_buff[0] = IPC::MakeHeader(0x40, 0x1, 0); //0x00400040
    cmd_buff[1] = SendCaptureBufferInfo(buffer_addr, buffer_size).raw;
    LOG_WARNING(Service_APT, "buffer_addr=0x%08X, buffer_size=0x%08X", buffer_addr, buffer_size);
}

ResultCode ReceiveCaptureBufferInfo(u32& buffer_addr, u32 buffer_size) {
    if (buffer_addr) {
        if (buffer_size > sizeof(CaptureBufferInfo)) {
            buffer_size = sizeof(CaptureBufferInfo);
        }
    } else {
        buffer_size = 0;
    }

    if (is_capture_buffer_info_set) {
        memcpy(Memory::GetPointer(buffer_addr), &capture_buffer_info, buffer_size);
    } else {
        InitCaptureBufferInfo();
        memcpy(Memory::GetPointer(buffer_addr), &capture_buffer_info, buffer_size);
    }

    return RESULT_SUCCESS;
}

void ReceiveCaptureBufferInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 buffer_size = cmd_buff[1];
    VAddr buffer_addr = cmd_buff[0x41];

    if (buffer_size > 0x1000) {
        buffer_size = 0x1000;
    }

    cmd_buff[0] = IPC::MakeHeader(0x41, 0x2, 0x2); //0x00410082
    cmd_buff[1] = ReceiveCaptureBufferInfo(buffer_addr, buffer_size).raw;
    cmd_buff[2] = buffer_size;
    cmd_buff[3] = (buffer_size << 14) | 2;
    LOG_WARNING(Service_APT, "buffer_size=0x%08X", buffer_size);
}

void GetCaptureInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 buffer_size = cmd_buff[1];
    VAddr buffer_addr = cmd_buff[0x41];

    if (buffer_size > 0x1000) {
        buffer_size = 0x1000;
    }

    cmd_buff[0] = IPC::MakeHeader(0x4A, 0x1, 0x2);
    cmd_buff[1] = ReceiveCaptureBufferInfo(buffer_addr, buffer_size).raw;
    cmd_buff[2] = (buffer_size << 14) | 2;
    LOG_WARNING(Service_APT, "buffer_size=0x%08X", buffer_size);
}

void NotifyToWait(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_APT, "(STUBBED) app_id=0x%08X", app_id);
}

void GetLockHandle(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // Bits [0:2] are the applet type (System, Library, etc)
    // Bit 5 tells the application that there's a pending APT parameter,
    // this will cause the app to wait until parameter_event is signaled.
    u32 applet_attributes = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    cmd_buff[2] = applet_attributes; // Applet Attributes, this value is passed to Enable.
    cmd_buff[3] = 0; // Least significant bit = power button state
    cmd_buff[4] = IPC::CopyHandleDesc();
    cmd_buff[5] = Kernel::g_handle_table.Create(lock).MoveFrom();

    LOG_WARNING(Service_APT, "(STUBBED) called handle=0x%08X applet_attributes=0x%08X", cmd_buff[5], applet_attributes);
}

void Enable(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 attributes = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    parameter_event->Signal(); // Let the application know that it has been started
    LOG_WARNING(Service_APT, "(STUBBED) called attributes=0x%08X", attributes);
}

void GetAppletManInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    AppletPos applet_pos = static_cast<AppletPos>(cmd_buff[1]);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u32>(applet_pos);
    cmd_buff[3] = 0;
    cmd_buff[4] = static_cast<u32>(AppletId::HomeMenu); // Home menu AppID
    cmd_buff[5] = static_cast<u32>(AppletId::Application); // TODO(purpasmart96): Do this correctly
    LOG_WARNING(Service_APT, "(STUBBED) called applet_pos=0x%08X", applet_pos);
}

void CountRegisteredApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = HLE::Applets::GetRegisteredAppletCount();
    LOG_WARNING(Service_APT, "registered_applet_count=0x%08X", cmd_buff[2]);
}

void IsRegistered(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    // TODO(Subv): An application is considered "registered" if it has already called APT::Enable
    // handle this properly once we implement multiprocess support.
    cmd_buff[2] = 0; // Set to not registered by default

    if (app_id == static_cast<u32>(AppletId::AnyLibraryApplet)) {
        cmd_buff[2] = HLE::Applets::IsLibraryAppletRunning() ? 1 : 0;
    } else if (auto applet = HLE::Applets::Applet::Get(static_cast<AppletId>(app_id))) {
        cmd_buff[2] = 1; // Set to registered
    }
    LOG_WARNING(Service_APT, "(STUBBED) called app_id=0x%08X", app_id);
}

void InquireNotification(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u32>(SignalType::None); // Signal type
    LOG_WARNING(Service_APT, "(STUBBED) called app_id=0x%08X", app_id);
}

void SendParameter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 src_app_id     = cmd_buff[1];
    u32 dst_app_id     = cmd_buff[2];
    u32 signal_type    = cmd_buff[3];
    u32 buffer_size    = cmd_buff[4];
    u32 value          = cmd_buff[5];
    u32 handle         = cmd_buff[6];
    u32 size           = cmd_buff[7];
    u32 buffer         = cmd_buff[8];

    std::shared_ptr<HLE::Applets::Applet> dest_applet = HLE::Applets::Applet::Get(static_cast<AppletId>(dst_app_id));

    if (dest_applet == nullptr) {
        LOG_ERROR(Service_APT, "Unknown applet id=0x%08X", dst_app_id);
        cmd_buff[1] = -1; // TODO(Subv): Find the right error code
        return;
    }

    MessageParameter param;
    param.destination_id = dst_app_id;
    param.sender_id = src_app_id;
    param.object = Kernel::g_handle_table.GetGeneric(handle);
    param.signal = signal_type;
    param.buffer.resize(buffer_size);
    Memory::ReadBlock(buffer, param.buffer.data(), param.buffer.size());

    cmd_buff[1] = dest_applet->ReceiveParameter(param).raw;

    LOG_WARNING(Service_APT, "(STUBBED) called src_app_id=0x%08X, dst_app_id=0x%08X, signal_type=0x%08X,"
               "buffer_size=0x%08X, value=0x%08X, handle=0x%08X, size=0x%08X, in_param_buffer_ptr=0x%08X",
               src_app_id, dst_app_id, signal_type, buffer_size, value, handle, size, buffer);
}

void ReceiveParameter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    u32 buffer_size = cmd_buff[2];
    VAddr buffer = cmd_buff[0x41];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = next_parameter.sender_id;
    cmd_buff[3] = next_parameter.signal; // Signal type
    cmd_buff[4] = next_parameter.buffer.size(); // Parameter buffer size
    cmd_buff[5] = 0x10;
    cmd_buff[6] = 0;
    if (next_parameter.object != nullptr)
        cmd_buff[6] = Kernel::g_handle_table.Create(next_parameter.object).MoveFrom();
    cmd_buff[7] = (next_parameter.buffer.size() << 14) | 2;
    cmd_buff[8] = buffer;

    Memory::WriteBlock(buffer, next_parameter.buffer.data(), next_parameter.buffer.size());

    LOG_WARNING(Service_APT, "called app_id=0x%08X, buffer_size=0x%08X", app_id, buffer_size);
}

void GlanceParameter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    u32 buffer_size = cmd_buff[2];
    VAddr buffer = cmd_buff[0x41];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = next_parameter.sender_id;
    cmd_buff[3] = next_parameter.signal; // Signal type
    cmd_buff[4] = next_parameter.buffer.size(); // Parameter buffer size
    cmd_buff[5] = 0x10;
    cmd_buff[6] = 0;
    if (next_parameter.object != nullptr)
        cmd_buff[6] = Kernel::g_handle_table.Create(next_parameter.object).MoveFrom();
    cmd_buff[7] = (next_parameter.buffer.size() << 14) | 2;
    cmd_buff[8] = buffer;

    Memory::WriteBlock(buffer, next_parameter.buffer.data(), std::min(static_cast<size_t>(buffer_size), next_parameter.buffer.size()));

    LOG_WARNING(Service_APT, "called app_id=0x%08X, buffer_size=0x%08X", app_id, buffer_size);
}

void CancelParameter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 check_sender   = cmd_buff[1] & 0xFF;
    u32 sender_id      = cmd_buff[2];
    u32 check_receiver = cmd_buff[3] & 0xFF;
    u32 receiver_id    = cmd_buff[4];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 1; // Set to Success

    LOG_WARNING(Service_APT, "(STUBBED) check_sender=0x%08X, sender_id=0x%08X, check_receiver=0x%08X, receiver_id=0x%08X",
                check_sender, sender_id, check_receiver, receiver_id);
}

void GetPreparationState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u32>(applet_preparation_state);
    LOG_WARNING(Service_APT, "(STUBBED) called");
}

void SetPreparationState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    applet_preparation_state = static_cast<AppletPreparationState>(cmd_buff[1]);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_APT, "(STUBBED) applet_preparation_state=0x%08X", applet_preparation_state);
}

void PrepareToStartApplication(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 title_info1  = cmd_buff[1];
    u32 title_info2  = cmd_buff[2];
    u32 title_info3  = cmd_buff[3];
    u32 title_info4  = cmd_buff[4];
    u32 flags        = cmd_buff[5];

    if (flags & 0x00000100) {
        unknown_ns_state_field = 1;
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_APT, "(STUBBED) called title_info1=0x%08X, title_info2=0x%08X, title_info3=0x%08X,"
               "title_info4=0x%08X, flags=0x%08X", title_info1, title_info2, title_info3, title_info4, flags);
}

void StartApplication(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 parameter_size   = cmd_buff[1];
    u32 hmac_size        = cmd_buff[2];
    u32 is_paused        = cmd_buff[3] & 0xFF;
    u32 parameter_transition = cmd_buff[4];
    u32 parameter_buffer = cmd_buff[5];
    u32 hmac_transtion   = cmd_buff[6];
    u32 hmac_buffer      = cmd_buff[7];

    if ((parameter_transition & 0x3C0F) != 2 || (hmac_transtion & 0x3C0F) != 0x802) {
        cmd_buff[0] = IPC::MakeHeader(0, 0x1, 0); // 0x40
        cmd_buff[1] = ResultCode(ErrorDescription::OS_InvalidBufferDescriptor, ErrorModule::OS,
                                 ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw; //0xD9001830
        return;
    }

    cmd_buff[0] = IPC::MakeHeader(0x1B, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_APT, "(STUBBED) parameter_size=0x%08X, hmac_size=0x%08X, is_paused=0x%08X, parameter_buffer=0x%08X, hmac_buffer=0x%08X",
                parameter_size, hmac_size, is_paused, parameter_buffer, hmac_buffer);
}

ResultCode AppletUtility(u32& utility_id, u32& input_buffer, u32& input_size, u32& output_buffer,u32& output_size, bool applet_result) {
    u32 sender_id = 0;
    u32 transition = 0;
    bool shell_open = true;

    switch (static_cast<UtilityID>(utility_id)) {
    case UtilityID::ClearPowerButtonState: // not useful
    case UtilityID::ClearExclusiveControl:
        break;

    case UtilityID::SleepIfShellClosed:
        shell_open = PTM::GetShellState();
        if (!shell_open) {
            LOG_WARNING(Service_APT, "(STUBBED) SleepIfShellClosed, The 3DS system should sleep now.");
        }
        else {
            LOG_WARNING(Service_APT, "(STUBBED) SleepIfShellClosed shell_open=%u", shell_open);
        }
        break;

    case UtilityID::LockTransition:
        transition = Memory::Read32(input_buffer);
        LOG_WARNING(Service_APT, "(STUBBED) LockTransition, transition=0x%08X", transition);
        break;

    case UtilityID::TryLockTransition:
        applet_result = false;
        if (output_buffer) {
            if (output_size != 0) {
                transition = Memory::Read32(input_buffer);
                Memory::Write32(output_buffer, 0);
                applet_result = true;
                LOG_WARNING(Service_APT, "(STUBBED) TryLockTransition, transition=0x%08X", transition);
            }
        }
        LOG_ERROR(Service_APT, "(STUBBED) Utility TryLockTransition Operation Failed");
        break;

    case UtilityID::UnlockTransition:
        transition = Memory::Read32(input_buffer);
        LOG_WARNING(Service_APT, "(STUBBED) UnlockTransition, transition=0x%08X", transition);
        break;

    case UtilityID::StartExitTask: // not useful
        break;

    case UtilityID::SetInitialSenderId:
        sender_id = Memory::Read32(input_buffer);
        LOG_WARNING(Service_APT, "(STUBBED) SetInitialSenderId, sender_id=0x%08X", sender_id);
        break;

    case UtilityID::SetPowerButtonClick: // not useful
        break;

    case UtilityID::SetHomeMenuBuffer:
        // This input_buffer content(may be a src_appid or a dst_appid)
        //      will be used by homemenu operation(like APT::LeaveHomeMenu) which try to do SendParameter()
        is_homemenu_input_buffer_set = true;
        homemenu_input_buffer = Memory::Read32(input_buffer);
        LOG_WARNING(Service_APT, "(STUBBED) SetLeaveHomeMenuBuffer, input_buffer=0x%08X", input_buffer);
        break;

    default:
        LOG_ERROR(Service_APT, "(STUBBED) unknown utility_id=0x%08X", utility_id);
        UNIMPLEMENTED();
    }
    return RESULT_SUCCESS;
}

void AppletUtility(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 utility_id        = cmd_buff[1];
    u32 input_size        = cmd_buff[2];
    u32 output_size       = (cmd_buff[3] > 0x1000) ? 0x1000 : cmd_buff[3];
    u32 input_traslation  = cmd_buff[4];
    u32 input_buffer      = cmd_buff[5];
    u32 output_traslation = cmd_buff[0x40];
    u32 output_buffer     = cmd_buff[0x41];

    bool applet_result = false; // Only use for utility_id = 6 (bool TryLockTransition)

    if (input_size > 0x1000) {
        cmd_buff[0] = IPC::MakeHeader(0, 0x1, 0); // 0x40
        cmd_buff[1] = ResultCode(ErrorDescription::OS_InvalidBufferDescriptor, ErrorModule::OS,
                                 ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw;;// 0xD9001830
        return;
    }

    cmd_buff[0] = IPC::MakeHeader(0x4B, 0x2, 0x2);// 0x004B0082
    cmd_buff[1] = AppletUtility(utility_id, input_buffer, input_size, output_buffer, output_size, applet_result).raw;
    cmd_buff[2] = static_cast<u32>(applet_result);
}

void SetAppCpuTimeLimit(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 value   = cmd_buff[1];
    cpu_percent = cmd_buff[2];

    if (value != 1) {
        LOG_ERROR(Service_APT, "This value should be one, but is actually %u!", value);
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_APT, "(STUBBED) called cpu_percent=%u, value=%u", cpu_percent, value);
}

void GetAppCpuTimeLimit(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 value = cmd_buff[1];

    if (value != 1) {
        LOG_ERROR(Service_APT, "This value should be one, but is actually %u!", value);
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = cpu_percent;

    LOG_WARNING(Service_APT, "(STUBBED) called value=%u", value);
}

void PrepareToStartLibraryApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    AppletId applet_id = static_cast<AppletId>(cmd_buff[1]);
    auto applet = HLE::Applets::Applet::Get(applet_id);
    if (applet) {
        LOG_WARNING(Service_APT, "applet has already been started id=%08X", applet_id);
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        cmd_buff[1] = HLE::Applets::Applet::Create(applet_id).raw;
    }
    LOG_DEBUG(Service_APT, "called applet_id=%08X", applet_id);
}

void PreloadLibraryApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    AppletId applet_id = static_cast<AppletId>(cmd_buff[1]);
    auto applet = HLE::Applets::Applet::Get(applet_id);
    if (applet) {
        LOG_WARNING(Service_APT, "applet has already been started id=%08X", applet_id);
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        cmd_buff[1] = HLE::Applets::Applet::Create(applet_id).raw;
    }
    LOG_DEBUG(Service_APT, "called applet_id=%08X", applet_id);
}

void StartLibraryApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    AppletId applet_id = static_cast<AppletId>(cmd_buff[1]);
    std::shared_ptr<HLE::Applets::Applet> applet = HLE::Applets::Applet::Get(applet_id);

    LOG_DEBUG(Service_APT, "called applet_id=%08X", applet_id);

    if (applet == nullptr) {
        LOG_ERROR(Service_APT, "unknown applet id=%08X", applet_id);
        cmd_buff[1] = -1; // TODO(Subv): Find the right error code
        return;
    }

    size_t buffer_size = cmd_buff[2];
    VAddr buffer_addr = cmd_buff[6];

    AppletStartupParameter parameter;
    parameter.object = Kernel::g_handle_table.GetGeneric(cmd_buff[4]);
    parameter.buffer.resize(buffer_size);
    Memory::ReadBlock(buffer_addr, parameter.buffer.data(), parameter.buffer.size());

    cmd_buff[1] = applet->Start(parameter).raw;
}

void SetScreenCapPostPermission(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    screen_capture_post_permission = static_cast<ScreencapPostPermission>(cmd_buff[1] & 0xF);

    cmd_buff[0] = IPC::MakeHeader(0x55, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_APT, "(STUBBED) screen_capture_post_permission=%u", screen_capture_post_permission);
}

void GetScreenCapPostPermission(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x56, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = static_cast<u32>(screen_capture_post_permission);
    LOG_WARNING(Service_APT, "(STUBBED) screen_capture_post_permission=%u", screen_capture_post_permission);
}

void PrepareToCloseApplication(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 cancel_preload = cmd_buff[1] & 0xFF;

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_APT, "(STUBBED) cancel_preload=%u", cancel_preload);
}

void CloseApplication(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 parameter_size = cmd_buff[1];
    u32 handle_parameter = cmd_buff[3];
    u32 parameter_buffer = cmd_buff[5];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_APT, "(STUBBED) parameter_size=0x%08X, handle_parameter=0x%08X, parameter_buffer=0x%08X",
                parameter_size, handle_parameter, parameter_buffer);
}

void LeaveHomeMenu(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 parameter_size = cmd_buff[1];
    u32 handle_parameter = cmd_buff[3];
    u32 parameter_buffer_transition = cmd_buff[4];
    u32 parameter_buffer = cmd_buff[5];

    if (cmd_buff[2] != 0 || (parameter_buffer_transition & 0x3C0F) != 2) {
        cmd_buff[0] = IPC::MakeHeader(0, 0x1, 0);// 0x40
        cmd_buff[1] = ResultCode(ErrorDescription::OS_InvalidBufferDescriptor, ErrorModule::OS,
                                 ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw; //0xD9001830
        return;
    }

    cmd_buff[0] = IPC::MakeHeader(0x2E, 0x1, 0); //0x2E0040
    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_APT, "(STUBBED) parameter_size=0x%08X, handle_parameter=0x%08X, parameter_buffer=0x%08X",
        parameter_size, handle_parameter, parameter_buffer);
}

void GetAppletInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    auto app_id = static_cast<AppletId>(cmd_buff[1]);

    if (auto applet = HLE::Applets::Applet::Get(app_id)) {
        // TODO(Subv): Get the title id for the current applet and write it in the response[2-3]
        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[4] = static_cast<u32>(Service::FS::MediaType::NAND);
        cmd_buff[5] = 1; // Registered
        cmd_buff[6] = 1; // Loaded
        cmd_buff[7] = 0; // Applet Attributes
    } else {
        cmd_buff[1] = ResultCode(ErrorDescription::NotFound, ErrorModule::Applet,
                                 ErrorSummary::NotFound, ErrorLevel::Status).raw;
    }
    LOG_WARNING(Service_APT, "(stubbed) called appid=%u", app_id);
}

void GetStartupArgument(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 parameter_size = cmd_buff[1];
    u32 parameter_buffer = cmd_buff[0x41];

    StartupArgumentType startup_argument_type = static_cast<StartupArgumentType>(cmd_buff[2]);

    if (parameter_size > 0x1000) {
        parameter_size = 0x1000;
    }

    LOG_WARNING(Service_APT,"(STUBBED) called startup_argument_type=%u , parameter_size=0x%08x , parameter_buffer=0x%08x",
                startup_argument_type, parameter_size, parameter_buffer);

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (parameter_size > 0) ? 1 : 0;
    cmd_buff[3] = (parameter_size << 14) | 2;
}

void CheckNew3DSApp(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    if (unknown_ns_state_field) {
        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[2] = 0;
    } else {
        PTM::CheckNew3DS(self);
    }

    cmd_buff[0] = IPC::MakeHeader(0x101, 2, 0);
    LOG_WARNING(Service_APT, "(STUBBED) called");
}

void CheckNew3DS(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    PTM::CheckNew3DS(self);

    cmd_buff[0] = IPC::MakeHeader(0x102, 2, 0);
    LOG_WARNING(Service_APT, "(STUBBED) called");
}

void IsStandardMemoryLayout(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const bool is_new_3ds = Settings::values.is_new_3ds;

    u32 is_standard_memory_layout = 0;

    using ConfigMem::config_mem;

    if (is_new_3ds) {
        if (config_mem.app_mem_type != 7) { // 7 for 178MB mode.
            is_standard_memory_layout = 1;
        }
    } else {
        if (config_mem.app_mem_type == 0) { // 0 for 64MB mode
            is_standard_memory_layout = 1;
        }
    }

    cmd_buff[0] = IPC::MakeHeader(0x104, 0x2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = is_standard_memory_layout;
    LOG_WARNING(Service_APT, "(STUBBED) is_standard_memory_layout=%u", is_standard_memory_layout);
}

void Init() {
    AddService(new APT_A_Interface);
    AddService(new APT_S_Interface);
    AddService(new APT_U_Interface);

    HLE::Applets::Init();

    // Load the shared system font (if available).
    // The expected format is a decrypted, uncompressed BCFNT file with the 0x80 byte header
    // generated by the APT:U service. The best way to get is by dumping it from RAM. We've provided
    // a homebrew app to do this: https://github.com/citra-emu/3dsutils. Put the resulting file
    // "shared_font.bin" in the Citra "sysdata" directory.

    std::string filepath = FileUtil::GetUserPath(D_SYSDATA_IDX) + SHARED_FONT;

    FileUtil::CreateFullPath(filepath); // Create path if not already created
    FileUtil::IOFile file(filepath, "rb");

    if (file.IsOpen()) {
        // Create shared font memory object
        using Kernel::MemoryPermission;
        shared_font_mem = Kernel::SharedMemory::Create(nullptr, 0x332000, // 3272 KB
                MemoryPermission::ReadWrite, MemoryPermission::Read, 0, Kernel::MemoryRegion::SYSTEM, "APT:SharedFont");
        // Read shared font data
        file.ReadBytes(shared_font_mem->GetPointer(), file.GetSize());
    } else {
        LOG_WARNING(Service_APT, "Unable to load shared font: %s", filepath.c_str());
        shared_font_mem = nullptr;
    }

    lock = Kernel::Mutex::Create(false, "APT_U:Lock");

    cpu_percent = 0;
    homemenu_input_buffer = 0;
    screen_capture_post_permission = ScreencapPostPermission::CleanThePermission; // TODO(JamePeng): verify the initial value

    // TODO(bunnei): Check if these are created in Initialize or on APT process startup.
    notification_event = Kernel::Event::Create(Kernel::ResetType::OneShot, "APT_U:Notification");
    parameter_event = Kernel::Event::Create(Kernel::ResetType::OneShot, "APT_U:Start");

    next_parameter.signal = static_cast<u32>(SignalType::AppJustStarted);
    next_parameter.destination_id = 0x300;
}

void Shutdown() {
    is_capture_buffer_info_set = false;
    is_homemenu_input_buffer_set = false;
    shared_font_mem = nullptr;
    shared_font_relocated = false;
    lock = nullptr;
    notification_event = nullptr;
    parameter_event = nullptr;

    next_parameter.object = nullptr;

    HLE::Applets::Shutdown();
}

} // namespace APT
} // namespace Service
