// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/kernel/event.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/cam_c.h"
#include "core/hle/service/cam/cam_q.h"
#include "core/hle/service/cam/cam_s.h"
#include "core/hle/service/cam/cam_u.h"
#include "core/hle/service/service.h"
#include "core/hle/service/y2r_u.h"

#include "core/hw/camera.h"

namespace Service {
namespace CAM {

void StartCapture(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x1, 1, 0);
    cmd_buff[1] = HW::Camera::StartCapture(port).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%u", port);
}

void StopCapture(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x2, 1, 0);
    cmd_buff[1] = HW::Camera::StopCapture(port).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%u", port);
}

void IsBusy(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;
    bool is_busy = false;

    cmd_buff[1] = HW::Camera::IsBusy(port, is_busy).raw;
    cmd_buff[2] = is_busy;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%u", port);
}

void ClearBuffer(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;

    cmd_buff[1] = HW::Camera::ClearBuffer(port).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%u", port);
}

void GetVsyncInterruptEvent(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;
    Handle event;

    cmd_buff[0] = IPC::MakeHeader(0x5, 1, 2);
    cmd_buff[1] = HW::Camera::GetVsyncInterruptEvent(port, event).raw;
    cmd_buff[2] = IPC::MoveHandleDesc();
    cmd_buff[3] = event;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%u", port);
}

void GetBufferErrorInterruptEvent(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;
    Handle event;

    cmd_buff[0] = IPC::MakeHeader(0x6, 1, 2);
    cmd_buff[1] = HW::Camera::GetBufferErrorInterruptEvent(port, event).raw;
    cmd_buff[2] = IPC::MoveHandleDesc();
    cmd_buff[3] = event;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%u", port);
}

void SetReceiving(Service::Interface* self) {
    u32* cmd_buff  = Kernel::GetCommandBuffer();

    VAddr dest     = cmd_buff[1];
    u8 port = cmd_buff[2] & 0xFF;
    u32 image_size = cmd_buff[3];
    u16 trans_unit = cmd_buff[4] & 0xFFFF;
    Handle event;


    cmd_buff[0] = IPC::MakeHeader(0x7, 1, 2);
    cmd_buff[1] = HW::Camera::SetReceiving(port, dest, image_size, trans_unit, event).raw;
    cmd_buff[2] = IPC::MoveHandleDesc();
    cmd_buff[3] = event;

    LOG_WARNING(Service_CAM, "(STUBBED) called, addr=0x%X, port=%u, image_size=%u, trans_unit=%u",
                dest, port, image_size, trans_unit);
}

void SetTransferLines(Service::Interface* self) {
    u32* cmd_buff      = Kernel::GetCommandBuffer();

    u32 port = cmd_buff[1] & 0xFF;
    u16 transfer_lines = cmd_buff[2] & 0xFFFF;
    u16 width = cmd_buff[3] & 0xFFFF;
    u16 height = cmd_buff[4] & 0xFFFF;

    cmd_buff[0] = IPC::MakeHeader(0x9, 1, 0);
    cmd_buff[1] = HW::Camera::SetTransferLines(port, transfer_lines, width, height).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%u, lines=%u, width=%u, height=%u",
                port, transfer_lines, width, height);
}

void GetMaxLines(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u16 width  = cmd_buff[1] & 0xFFFF;
    u16 height = cmd_buff[2] & 0xFFFF;

    cmd_buff[0] = IPC::MakeHeader(0xA, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = HW::Camera::TRANSFER_BUFFER_SIZE / (2 * width);

    LOG_WARNING(Service_CAM, "(STUBBED) called, width=%u, height=%u, lines = %u",
                width, height, cmd_buff[2]);
}

void SetTransferBytes(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;
    u32 transfer_bytes = cmd_buff[2];
    u16 width = cmd_buff[3] & 0xFFFF;
    u16 height = cmd_buff[4] & 0xFFFF;

    cmd_buff[1] = HW::Camera::SetTransferBytes(port, transfer_bytes, width, height).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%u, transfer_bytes=%u, width=%u, height=%u",
                port, transfer_bytes, width, height);
}

void GetTransferBytes(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;
    u32 transfer_bytes;

    cmd_buff[0] = IPC::MakeHeader(0xC, 2, 0);
    cmd_buff[1] = HW::Camera::GetTransferBytes(port, transfer_bytes).raw;
    cmd_buff[2] = transfer_bytes;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%d", port);
}

void SetTrimming(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;
    bool trim = (cmd_buff[2] & 0xFF) != 0;

    cmd_buff[0] = IPC::MakeHeader(0xE, 1, 0);
    cmd_buff[1] = HW::Camera::SetTrimming(port, trim).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%d, trim=%d", port, trim);
}

void SetTrimmingParams(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8  port = cmd_buff[1] & 0xFF;
    s16 x_start = cmd_buff[2] & 0xFFFF;
    s16 y_start = cmd_buff[3] & 0xFFFF;
    s16 x_end = cmd_buff[4] & 0xFFFF;
    s16 y_end = cmd_buff[5] & 0xFFFF;

    cmd_buff[1] = HW::Camera::SetTrimmingParams(port, x_start, y_start, x_end, y_end).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%d, x_start=%d, y_start=%d, x_end=%d, y_end=%d",
        port, x_start, y_start, x_end, y_end);
}

void SetTrimmingParamsCenter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8  port  = cmd_buff[1] & 0xFF;
    s16 trimW = cmd_buff[2] & 0xFFFF;
    s16 trimH = cmd_buff[3] & 0xFFFF;
    s16 camW  = cmd_buff[4] & 0xFFFF;
    s16 camH  = cmd_buff[5] & 0xFFFF;

    cmd_buff[0] = IPC::MakeHeader(0x12, 1, 0);
    cmd_buff[1] = HW::Camera::SetTrimmingParamsCenter(port, trimW, trimH, camW, camH).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%d, trimW=%d, trimH=%d, camW=%d, camH=%d",
                port, trimW, trimH, camW, camH);
}

void Activate(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 camera_select = cmd_buff[1] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x13, 1, 0);
    cmd_buff[1] = HW::Camera::Activate(camera_select).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, cam_select=%u", camera_select);
}

void FlipImage(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 cam_select = cmd_buff[1] & 0xFF;
    u8 flip       = cmd_buff[2] & 0xFF;
    u8 context    = cmd_buff[3] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x1D, 1, 0);
    cmd_buff[1] = HW::Camera::FlipImage(cam_select, flip, context).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, cam_select=%u, flip=%u, context=%u",
                cam_select, flip, context);
}

void SetDetailSize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 cam_select = cmd_buff[1] & 0xFF;
    s16 width = cmd_buff[2] & 0xFFFF;
    s16 height = cmd_buff[3] & 0xFFFF;
    s16 cropX0 = cmd_buff[4] & 0xFFFF;
    s16 cropY0 = cmd_buff[5] & 0xFFFF;
    s16 cropX1 = cmd_buff[6] & 0xFFFF;
    s16 cropY1 = cmd_buff[7] & 0xFFFF;
    u8 context = cmd_buff[8] & 0xFF;

    cmd_buff[1] = HW::Camera::SetDetailSize(cam_select, width, height, cropX0, cropY0, cropX1, cropY1, context).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, cam_select=%u, width=%d, height=%d, cropX0=%d, cropY0=%d, cropX1=%d, cropY1=%d, context=%u",
                cam_select, width, height, cropX0, cropY0, cropX1, cropY1, context);
}

void SetSize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 cam_select = cmd_buff[1] & 0xFF;
    u8 size       = cmd_buff[2] & 0xFF;
    u8 context    = cmd_buff[3] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x1F, 1, 0);
    cmd_buff[1] = HW::Camera::SetSize(cam_select, size, context).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, cam_select=%u, size=%u, context=%u",
                cam_select, size, context);
}

void SetFrameRate(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 cam_select = cmd_buff[1] & 0xFF;
    u8 frame_rate = cmd_buff[2] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x20, 1, 0);
    cmd_buff[1] = HW::Camera::SetFrameRate(cam_select, frame_rate).raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, cam_select=%d, frame_rate=%d",
                cam_select, frame_rate);
}

void GetStereoCameraCalibrationData(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // Default values taken from yuriks' 3DS. Valid data is required here or games using the
    // calibration get stuck in an infinite CPU loop.
    HW::Camera::StereoCameraCalibrationData data = {};
    data.isValidRotationXY = 0;
    data.scale = 1.001776f;
    data.rotationZ = 0.008322907f;
    data.translationX = -87.70484f;
    data.translationY = -7.640977f;
    data.rotationX = 0.0f;
    data.rotationY = 0.0f;
    data.angleOfViewRight = 64.66875f;
    data.angleOfViewLeft = 64.76067f;
    data.distanceToChart = 250.0f;
    data.distanceCameras = 35.0f;
    data.imageWidth = 640;
    data.imageHeight = 480;

    cmd_buff[0] = IPC::MakeHeader(0x2B, 17, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    memcpy(&cmd_buff[2], &data, sizeof(data));

    LOG_TRACE(Service_CAM, "called");
}

void GetSuitableY2rStandardCoefficient(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x36, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = static_cast<u32>(Y2R_U::StandardCoefficient::ITU_Rec601);

    LOG_WARNING(Service_CAM, "(STUBBED) called");
}

void PlayShutterSound(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 sound_id = cmd_buff[1] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x38, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, sound_id=%d", sound_id);
}

void DriverInitialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x39, 1, 0);
    cmd_buff[1] = HW::Camera::DriverInitialize().raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called");
}

void DriverFinalize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x3A, 1, 0);
    cmd_buff[1] = HW::Camera::DriverFinalize().raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called");
}

void Init() {
    using namespace Kernel;

    AddService(new CAM_C_Interface);
    AddService(new CAM_Q_Interface);
    AddService(new CAM_S_Interface);
    AddService(new CAM_U_Interface);

}

void Shutdown() {
}

} // namespace CAM
} // namespace Service
