// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"

#include "core/hle/hle.h"
#include "core/hle/result.h"

namespace HW {
namespace Camera {

enum class Port : u8 {
    None = 0,
    Cam1 = 1,
    Cam2 = 2,
    Both = Cam1 | Cam2
};

enum class CameraSelect : u8 {
    None = 0,
    Out1 = 1,
    In1 = 2,
    Out2 = 4,
    In1Out1 = Out1 | In1,
    Out1Out2 = Out1 | Out2,
    In1Out2 = In1 | Out2,
    All = Out1 | In1 | Out2
};

enum class Effect : u8 {
    None = 0,
    Mono = 1,
    Sepia = 2,
    Negative = 3,
    Negafilm = 4,
    Sepia01 = 5
};

enum class Context : u8 {
    None = 0,
    A = 1,
    B = 2,
    Both = A | B
};

enum class Flip : u8 {
    None = 0,
    Horizontal = 1,
    Vertical = 2,
    Reverse = 3
};

enum class Size : u8 {
    VGA = 0,
    QVGA = 1,
    QQVGA = 2,
    CIF = 3,
    QCIF = 4,
    DS_LCD = 5,
    DS_LCDx4 = 6,
    CTR_TOP_LCD = 7,
    CTR_BOTTOM_LCD = QVGA
};

enum class FrameRate : u8 {
    Rate_15 = 0,
    Rate_15_To_5 = 1,
    Rate_15_To_2 = 2,
    Rate_10 = 3,
    Rate_8_5 = 4,
    Rate_5 = 5,
    Rate_20 = 6,
    Rate_20_To_5 = 7,
    Rate_30 = 8,
    Rate_30_To_5 = 9,
    Rate_15_To_10 = 10,
    Rate_20_To_10 = 11,
    Rate_30_To_10 = 12
};

enum class ShutterSoundType : u8 {
    Normal = 0,
    Movie = 1,
    MovieEnd = 2
};

enum class WhiteBalance : u8 {
    BalanceAuto = 0,
    Balance3200K = 1,
    Balance4150K = 2,
    Balance5200K = 3,
    Balance6000K = 4,
    Balance7000K = 5,
    BalanceMax = 6,
    BalanceNormal = BalanceAuto,
    BalanceTungsten = Balance3200K,
    BalanceWhiteFluorescentLight = Balance4150K,
    BalanceDaylight = Balance5200K,
    BalanceCloudy = Balance6000K,
    BalanceHorizon = Balance6000K,
    BalanceShade = Balance7000K
};

enum class PhotoMode : u8 {
    Normal = 0,
    Portrait = 1,
    Landscape = 2,
    Nightview = 3,
    Letter0 = 4
};

enum class LensCorrection : u8 {
    Off = 0,
    On70 = 1,
    On90 = 2,
    Dark = Off,
    Normal = On70,
    Bright = On90
};

enum class Contrast : u8 {
    Pattern01 = 1,
    Pattern02 = 2,
    Pattern03 = 3,
    Pattern04 = 4,
    Pattern05 = 5,
    Pattern06 = 6,
    Pattern07 = 7,
    Pattern08 = 8,
    Pattern09 = 9,
    Pattern10 = 10,
    Pattern11 = 11,
    Low = Pattern05,
    Normal = Pattern06,
    High = Pattern07
};

enum class OutputFormat : u8 {
    YUV422 = 0,
    RGB565 = 1
};

const u32 TRANSFER_BUFFER_SIZE = 5 * 1024;

/// Stereo camera calibration data.
struct StereoCameraCalibrationData {
    u8 isValidRotationXY;      ///< Bool indicating whether the X and Y rotation data is valid.
    INSERT_PADDING_BYTES(3);
    float_le scale;            ///< Scale to match the left camera image with the right.
    float_le rotationZ;        ///< Z axis rotation to match the left camera image with the right.
    float_le translationX;     ///< X axis translation to match the left camera image with the right.
    float_le translationY;     ///< Y axis translation to match the left camera image with the right.
    float_le rotationX;        ///< X axis rotation to match the left camera image with the right.
    float_le rotationY;        ///< Y axis rotation to match the left camera image with the right.
    float_le angleOfViewRight; ///< Right camera angle of view.
    float_le angleOfViewLeft;  ///< Left camera angle of view.
    float_le distanceToChart;  ///< Distance between cameras and measurement chart.
    float_le distanceCameras;  ///< Distance between left and right cameras.
    s16_le imageWidth;         ///< Image width.
    s16_le imageHeight;        ///< Image height.
    INSERT_PADDING_BYTES(16);
};
static_assert(sizeof(StereoCameraCalibrationData) == 64, "StereoCameraCalibrationData structure size is wrong");

struct PackageParameterCameraSelect {
    CameraSelect camera;
    s8 exposure;
    WhiteBalance white_balance;
    s8 sharpness;
    bool auto_exposure;
    bool auto_white_balance;
    FrameRate frame_rate;
    PhotoMode photo_mode;
    Contrast contrast;
    LensCorrection lens_correction;
    bool noise_filter;
    u8 padding;
    s16 auto_exposure_window_x;
    s16 auto_exposure_window_y;
    s16 auto_exposure_window_width;
    s16 auto_exposure_window_height;
    s16 auto_white_balance_window_x;
    s16 auto_white_balance_window_y;
    s16 auto_white_balance_window_width;
    s16 auto_white_balance_window_height;
};

static_assert(sizeof(PackageParameterCameraSelect) == 28, "PackageParameterCameraSelect structure size is wrong");

void SignalVblankInterrupt();

ResultCode StartCapture(u8 port);
ResultCode StopCapture(u8 port);
ResultCode IsBusy(u8 port, bool& is_busy);
ResultCode ClearBuffer(u8 port);
ResultCode GetVsyncInterruptEvent(u8 port, Handle&  event);
ResultCode GetBufferErrorInterruptEvent(u8 port, Handle&  event);
ResultCode SetReceiving(u8 port, VAddr dest, u32 image_size, u16 trans_unit, Handle& event);
ResultCode SetTransferLines(u8 port, u16 transfer_lines, u16 width, u16 height);
ResultCode SetTransferBytes(u8 port, u32 transfer_bytes, u16 width, u16 height);
ResultCode GetTransferBytes(u8 port, u32& transfer_bytes);
ResultCode SetTrimming(u8 port, bool trimming);
ResultCode SetTrimmingParams(u8 port, s16 x_start, s16 y_start, s16 x_end, s16 y_end);
ResultCode SetTrimmingParamsCenter(u8 port, s16 trimW, s16 trimH, s16 camW, s16 camH);
ResultCode FlipImage(u8 camera_select, u8 flip, u8 context);
ResultCode SetSize(u8 camera_select, u8 size, u8 context);
ResultCode SetDetailSize(u8 camera_select, s16 width, s16 height, s16 cropX0, s16 cropY0, s16 cropX1, s16 cropY1, u8 context);
ResultCode SetFrameRate(u8 camera_select, u8 frame_rt);
ResultCode Activate(u8 camera_select);

ResultCode DriverInitialize();
ResultCode DriverFinalize();

/// Initialize hardware
void Init();

/// Shutdown hardware
void Shutdown();

} // namespace Camera
} // namespace HW
