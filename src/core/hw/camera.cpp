// Copyright 201 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/event.h"

#include "core/hw/camera.h"
#include <memory>

namespace HW {
namespace Camera {

struct CameraContext {
    Size size;
    Effect effect;
    Flip flip;
    s16 width;
    s16 height;
    s16 cropX0;
    s16 cropY0;
    s16 cropX1;
    s16 cropY1;
};

struct CameraConfig {
    FrameRate frame_rate;
    Context current_context = Context::A;
    CameraContext context_A;
    CameraContext context_B;
};

struct PortConfig {
    bool is_capture = false;
    bool is_busy = false;
    u32 image_size;
    u32 trans_unit;
    VAddr dest;
    u16 transfer_lines;
    u16 width;
    u16 height;
    u32 transfer_bytes;
    bool trimming;
    s16 x_start;
    s16 y_start;
    s16 x_end;
    s16 y_end;
    s16 trimW;
    s16 trimH;
    s16 camW;
    s16 camH;

    Kernel::SharedPtr<Kernel::Event> completion_event_cam = Kernel::Event::Create(Kernel::ResetType::OneShot, "CAM_U::completion_event_cam");
    Kernel::SharedPtr<Kernel::Event> interrupt_buffer_error_event = Kernel::Event::Create(Kernel::ResetType::OneShot, "CAM_U::interrupt_buffer_error_event");
    Kernel::SharedPtr<Kernel::Event> vsync_interrupt_event = Kernel::Event::Create(Kernel::ResetType::OneShot, "CAM_U::vsync_interrupt_event");

    ~PortConfig() {
        completion_event_cam = nullptr;
        interrupt_buffer_error_event = nullptr;
        vsync_interrupt_event = nullptr;
    }
};

static bool driver_initialized = false;
static bool activated = false;
static bool camera_capture = false;

static FrameRate frame_rate = FrameRate::Rate_15;

static Port current_port = Port::None;
static std::unique_ptr<PortConfig> port1;
static std::unique_ptr<PortConfig> port2;

static CameraSelect current_camera = CameraSelect::None;
static CameraConfig inner;
static CameraConfig outer1;
static CameraConfig outer2;

void SignalVblankInterrupt() {
    if (driver_initialized && activated) {
        static u64 cycles = 0;
        cycles = (cycles + 1) % 60;
        bool signal = false;

        switch (frame_rate) {
        case FrameRate::Rate_30:
        case FrameRate::Rate_30_To_5:
        case FrameRate::Rate_30_To_10:
            signal = (cycles % 2) == 0;
            break;
        case FrameRate::Rate_20:
        case FrameRate::Rate_20_To_5:
        case FrameRate::Rate_20_To_10:
            signal = (cycles % 3) == 0;
            break;
        case FrameRate::Rate_15:
        case FrameRate::Rate_15_To_5:
        case FrameRate::Rate_15_To_2:
        case FrameRate::Rate_15_To_10:
            signal = (cycles % 4) == 0;
            break;
        case FrameRate::Rate_10:
            signal = (cycles % 6) == 0;
            break;
        case FrameRate::Rate_5:
        case FrameRate::Rate_8_5:
            signal = (cycles % 12) == 0;
            break;
        }

        if (signal) {
            switch(current_camera) {
            case CameraSelect::In1:
            case CameraSelect::Out2:
            case CameraSelect::In1Out2:
                port1->vsync_interrupt_event->Signal();
                break;
            case CameraSelect::Out1:
                port2->vsync_interrupt_event->Signal();
                break;
            case CameraSelect::In1Out1:
            case CameraSelect::Out1Out2:
            case CameraSelect::All:
                port1->vsync_interrupt_event->Signal();
                port2->vsync_interrupt_event->Signal();
                break;
            case CameraSelect::None:
                break;
            }
        }
    }
}

PortConfig* GetPort(u8 port) {
    switch (static_cast<Port>(port)) {
    case Port::Cam1:
        return port1.get();
    case Port::Cam2:
        return port2.get();
    default:
        return nullptr;
    }
}

ResultCode StartCapture(u8 port) {
    current_port = static_cast<Port>(port);

    if (port & (u8)Port::Cam1) {
        port1->completion_event_cam->Signal();
        port1->is_capture = true;
    }

    if (port & (u8)Port::Cam2) {
        port2->completion_event_cam->Signal();
        port2->is_capture = true;
    }
    return RESULT_SUCCESS;
}

ResultCode StopCapture(u8 port) {
    if (port & (u8)Port::Cam1) {
        port1->completion_event_cam->Clear();
        port1->is_capture = false;
    }

    if (port & (u8)Port::Cam2) {
        port2->completion_event_cam->Clear();
        port2->is_capture = false;
    }
    return RESULT_SUCCESS;
}

ResultCode IsBusy(u8 port, bool& is_busy) {

    PortConfig* port_config = GetPort(port);
    if(port_config) {
        is_busy = port_config->is_busy;
    }
    return RESULT_SUCCESS;
}

ResultCode ClearBuffer(u8 port) {
    if (port & (u8)Port::Cam1) {
    }

    if (port & (u8)Port::Cam2) {
    }
    return RESULT_SUCCESS;
}

ResultCode GetVsyncInterruptEvent(u8 port, Handle& event) {
    PortConfig* port_config = GetPort(port);
    if (port_config) {
        event = Kernel::g_handle_table.Create(port_config->vsync_interrupt_event).MoveFrom();
    }
    return RESULT_SUCCESS;
}

ResultCode GetBufferErrorInterruptEvent(u8 port, Handle& event) {
    PortConfig* port_config = GetPort(port);
    if (port_config) {
        event = Kernel::g_handle_table.Create(port_config->interrupt_buffer_error_event).MoveFrom();
    }
    return RESULT_SUCCESS;
}

ResultCode SetReceiving(u8 port, VAddr dest, u32 image_size, u16 trans_unit, Handle& event) {
    PortConfig* port_config = GetPort(port);
    if(port_config) {
        port_config->completion_event_cam->Signal();
        event = Kernel::g_handle_table.Create(port_config->completion_event_cam).MoveFrom();
        port_config->image_size = image_size;
        port_config->trans_unit = trans_unit;
        port_config->dest = dest;
    }
    return RESULT_SUCCESS;
}

ResultCode SetTransferLines(u8 port, u16 transfer_lines, u16 width, u16 height) {
    if (port & (u8)Port::Cam1) {
        port1->transfer_lines = transfer_lines;
        port1->width = width;
        port1->height = height;
    }

    if (port & (u8)Port::Cam2) {
        port2->transfer_lines = transfer_lines;
        port2->width = width;
        port2->height = height;
    }
    return RESULT_SUCCESS;
}

ResultCode SetTransferBytes(u8 port, u32 transfer_bytes, u16 width, u16 height) {
    if (port & (u8)Port::Cam1) {
        port1->transfer_bytes = transfer_bytes;
        port1->width = width;
        port1->height = height;
    }

    if (port & (u8)Port::Cam2) {
        port2->transfer_bytes = transfer_bytes;
        port2->width = width;
        port2->height = height;
    }
    return RESULT_SUCCESS;
}

ResultCode GetTransferBytes(u8 port, u32& transfer_bytes) {
    PortConfig* port_config = GetPort(port);
    if (port_config) {
        transfer_bytes = port_config->transfer_bytes;
    }
    return RESULT_SUCCESS;
}

ResultCode SetTrimming(u8 port, bool trimming) {
    if (port & (u8)Port::Cam1) {
        port1->trimming = trimming;
    }

    if (port & (u8)Port::Cam2) {
        port2->trimming = trimming;
    }
    return RESULT_SUCCESS;
}

ResultCode SetTrimmingParams(u8 port, s16 x_start, s16 y_start, s16 x_end, s16 y_end) {
    if (port & (u8)Port::Cam1) {
        port1->x_start = x_start;
        port1->y_start = y_start;
        port1->x_end = x_end;
        port1->y_end = y_end;
    }

    if (port & (u8)Port::Cam2) {
        port2->x_start = x_start;
        port2->y_start = y_start;
        port2->x_end = x_end;
        port2->y_end = y_end;
    }
    return RESULT_SUCCESS;
}

ResultCode SetTrimmingParamsCenter(u8 port, s16 trimW, s16 trimH, s16 camW, s16 camH) {
    if (port & (u8)Port::Cam1) {
        port1->trimW = trimW;
        port1->trimH = trimH;
        port1->camW = camW;
        port1->camH = camH;
    }

    if (port & (u8)Port::Cam2) {
        port2->trimW = trimW;
        port2->trimH = trimH;
        port2->camW = camW;
        port2->camH = camH;
    }
    return RESULT_SUCCESS;
}

ResultCode FlipImage(u8 camera_select, u8 flip, u8 context) {
    if (camera_select & (u8)CameraSelect::In1) {
        if(context & (u8)Context::A) {
            inner.context_A.flip = static_cast<Flip>(flip);
        }
        if (context & (u8)Context::B) {
            inner.context_B.flip = static_cast<Flip>(flip);
        }
    }
    if (camera_select & (u8)CameraSelect::Out1) {
        if (context & (u8)Context::A) {
            outer1.context_A.flip = static_cast<Flip>(flip);
        }
        if (context & (u8)Context::B) {
            outer1.context_B.flip = static_cast<Flip>(flip);
        }
    }
    if (camera_select & (u8)CameraSelect::Out2) {
        if (context & (u8)Context::A) {
            outer2.context_A.flip = static_cast<Flip>(flip);
        }
        if (context & (u8)Context::B) {
            outer2.context_B.flip = static_cast<Flip>(flip);
        }
    }
    return RESULT_SUCCESS;
}

ResultCode SetSize(u8 camera_select, u8 size, u8 context) {
    if (camera_select & (u8)CameraSelect::In1) {
        if (context & (u8)Context::A) {
            inner.context_A.size = static_cast<Size>(size);
        }
        if (context & (u8)Context::B) {
            inner.context_B.size = static_cast<Size>(size);
        }
    }
    if (camera_select & (u8)CameraSelect::Out1) {
        if (context & (u8)Context::A) {
            outer1.context_A.size = static_cast<Size>(size);
        }
        if (context & (u8)Context::B) {
            outer1.context_B.size = static_cast<Size>(size);
        }
    }
    if (camera_select & (u8)CameraSelect::Out2) {
        if (context & (u8)Context::A) {
            outer2.context_A.size = static_cast<Size>(size);
        }
        if (context & (u8)Context::B) {
            outer2.context_B.size = static_cast<Size>(size);
        }
    }
    return RESULT_SUCCESS;
}

ResultCode SetDetailSize(u8 camera_select, s16 width, s16 height, s16 cropX0, s16 cropY0, s16 cropX1, s16 cropY1, u8 context) {
    if (camera_select & (u8)CameraSelect::In1) {
        if (context & (u8)Context::A) {
            inner.context_A.width = width;
            inner.context_A.height = height;
            inner.context_A.cropX0 = cropX0;
            inner.context_A.cropY0 = cropY0;
            inner.context_A.cropX1 = cropX1;
            inner.context_A.cropY1 = cropY1;
        }
        if (context & (u8)Context::B) {
            inner.context_B.width = width;
            inner.context_B.height = height;
            inner.context_B.cropX0 = cropX0;
            inner.context_B.cropY0 = cropY0;
            inner.context_B.cropX1 = cropX1;
            inner.context_B.cropY1 = cropY1;
        }
    }
    if (camera_select & (u8)CameraSelect::Out1) {
        if (context & (u8)Context::A) {
            outer1.context_A.width = width;
            outer1.context_A.height = height;
            outer1.context_A.cropX0 = cropX0;
            outer1.context_A.cropY0 = cropY0;
            outer1.context_A.cropX1 = cropX1;
            outer1.context_A.cropY1 = cropY1;
        }
        if (context & (u8)Context::B) {
            outer1.context_B.width = width;
            outer1.context_B.height = height;
            outer1.context_B.cropX0 = cropX0;
            outer1.context_B.cropY0 = cropY0;
            outer1.context_B.cropX1 = cropX1;
            outer1.context_B.cropY1 = cropY1;
        }
    }
    if (camera_select & (u8)CameraSelect::Out2) {
        if (context & (u8)Context::A) {
            outer2.context_A.width = width;
            outer2.context_A.height = height;
            outer2.context_A.cropX0 = cropX0;
            outer2.context_A.cropY0 = cropY0;
            outer2.context_A.cropX1 = cropX1;
            outer2.context_A.cropY1 = cropY1;
        }
        if (context & (u8)Context::B) {
            outer2.context_B.width = width;
            outer2.context_B.height = height;
            outer2.context_B.cropX0 = cropX0;
            outer2.context_B.cropY0 = cropY0;
            outer2.context_B.cropX1 = cropX1;
            outer2.context_B.cropY1 = cropY1;
        }
    }
    return RESULT_SUCCESS;
}

ResultCode SetFrameRate(u8 camera_select, u8 frame_rt) {
    frame_rate = static_cast<FrameRate>(frame_rt);
    if (camera_select & (u8)CameraSelect::In1) {
        inner.frame_rate = static_cast<FrameRate>(frame_rt);
    }
    if (camera_select & (u8)CameraSelect::Out1) {
        outer1.frame_rate = static_cast<FrameRate>(frame_rt);
    }
    if (camera_select & (u8)CameraSelect::Out2) {
        outer2.frame_rate = static_cast<FrameRate>(frame_rt);
    }
    return RESULT_SUCCESS;
}


ResultCode Activate(u8 camera_select) {
    current_camera = static_cast<CameraSelect>(camera_select);
    activated = (current_camera == CameraSelect::None) ? false : true;
    return RESULT_SUCCESS;
}

ResultCode DriverInitialize() {
    activated = false;
    camera_capture = false;
    driver_initialized = true;
    return RESULT_SUCCESS;
}

ResultCode DriverFinalize() {
    activated = false;
    camera_capture = false;
    driver_initialized = false;
    return RESULT_SUCCESS;
}

void Init() {
    port1 = std::make_unique<PortConfig>();
    port2 = std::make_unique<PortConfig>();
}

void Shutdown() {
    port1 = nullptr;
    port2 = nullptr;
}

} // namespace Camera
} // namespace HW
