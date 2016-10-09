// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Service {

class Interface;

namespace CAM {

/**
 * StartCapture service function
 *  Inputs:
 *      0: 0x00010040
 *      1: u8 Camera port (`Port` enum)
 *  Outputs:
 *      0: 0x00010040
 *      1: ResultCode
 */
void StartCapture(Service::Interface* self);

/**
 * StopCapture service function
 *  Inputs:
 *      0: 0x00020040
 *      1: u8 Camera port (`Port` enum)
 *  Outputs:
 *      0: 0x00020040
 *      1: ResultCode
 */
void StopCapture(Service::Interface* self);

/**
 * IsBusy service function
 *  Inputs:
 *      0: 0x00030040
 *      1: u8 Camera port (`Port` enum)
 *  Outputs:
 *      0: 0x00030080
 *      1: ResultCode
 *      2: u8, 0 = Not Busy, 1 = Busy
 */
void IsBusy(Service::Interface* self);

/**
 * ClearBuffer service function
 *  Inputs:
 *      0: 0x00040040
 *      1: u8 Camera port (`Port` enum)
 *  Outputs:
 *      0: 0x00040040
 *      1: ResultCode
 */
void ClearBuffer(Service::Interface* self);

/**
 * GetVsyncInterruptEvent service function
 *  Inputs:
 *      0: 0x00050040
 *      1: u8 Camera port (`Port` enum)
 *  Outputs:
 *      0: 0x00050042
 *      1: ResultCode
 *      2: Descriptor: Handle
 *      3: Event handle
 */
void GetVsyncInterruptEvent(Service::Interface* self);

/**
 * GetBufferErrorInterruptEvent service function
 *  Inputs:
 *      0: 0x00060040
 *      1: u8 Camera port (`Port` enum)
 *  Outputs:
 *      0: 0x00060042
 *      1: ResultCode
 *      2: Descriptor: Handle
 *      3: Event handle
 */
void GetBufferErrorInterruptEvent(Service::Interface* self);

/**
 * Sets the target buffer to receive a frame of image data and starts the transfer. Each camera
 * port has its own event to signal the end of the transfer.
 *
 *  Inputs:
 *      0: 0x00070102
 *      1: Destination address in calling process
 *      2: u8 Camera port (`Port` enum)
 *      3: Image size (in bytes?)
 *      4: u16 Transfer unit size (in bytes?)
 *      5: Descriptor: Handle
 *      6: Handle to destination process
 *  Outputs:
 *      0: 0x00070042
 *      1: ResultCode
 *      2: Descriptor: Handle
 *      3: Handle to event signalled when transfer finishes
 */
void SetReceiving(Service::Interface* self);

/**
 * SetTransferLines service function
 *  Inputs:
 *      0: 0x00090100
 *      1: u8 Camera port (`Port` enum)
 *      2: u16 Number of lines to transfer
 *      3: u16 Width
 *      4: u16 Height
 *  Outputs:
 *      0: 0x00090040
 *      1: ResultCode
 */
void SetTransferLines(Service::Interface* self);

/**
 * GetMaxLines service function
 *  Inputs:
 *      0: 0x000A0080
 *      1: u16 Width
 *      2: u16 Height
 *  Outputs:
 *      0: 0x000A0080
 *      1: ResultCode
 *      2: Maximum number of lines that fit in the buffer(?)
 */
void GetMaxLines(Service::Interface* self);

/**
 * SetTransferBytes service function
 *  Inputs:
 *      0: 0x000B0100
 *      1: u8 Camera port (`Port` enum)
 *      2: Bytes
 *      3: u16 Width
 *      4: u16 Height
 *  Outputs:
 *      0: 0x000B0040
 *      1: ResultCode
 */
void SetTransferBytes(Service::Interface* self);

/**
 * GetTransferBytes service function
 *  Inputs:
 *      0: 0x000C0040
 *      1: u8 Camera port (`Port` enum)
 *  Outputs:
 *      0: 0x000C0080
 *      1: ResultCode
 *      2: Total number of bytes for each frame with current settings(?)
 */
void GetTransferBytes(Service::Interface* self);

/**
 * SetTrimming service function
 *  Inputs:
 *      0: 0x000E0080
 *      1: u8 Camera port (`Port` enum)
 *      2: u8 bool Enable trimming if true
 *  Outputs:
 *      0: 0x000E0040
 *      1: ResultCode
 */
void SetTrimming(Service::Interface* self);

/**
 * SetTrimmingParams service function
 *  Inputs:
 *      0: 0x00100140
 *      1: u8 Camera port (`Port` enum)
 *      2: s16 X Start
 *      3: s16 Y Start
 *      4: s16 X End
 *      5: s16 Y End
 *  Outputs:
 *      0: 0x00100040
 *      1: ResultCode
 */
void SetTrimmingParams(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x00120140
 *      1: u8 Camera port (`Port` enum)
 *      2: s16 Trim width(?)
 *      3: s16 Trim height(?)
 *      4: s16 Camera width(?)
 *      5: s16 Camera height(?)
 *  Outputs:
 *      0: 0x00120040
 *      1: ResultCode
 */
void SetTrimmingParamsCenter(Service::Interface* self);

/**
 * Selects up to two physical cameras to enable.
 *  Inputs:
 *      0: 0x00130040
 *      1: u8 Cameras to activate (`CameraSelect` enum)
 *  Outputs:
 *      0: 0x00130040
 *      1: ResultCode
 */
void Activate(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x001D00C0
 *      1: u8 Camera select (`CameraSelect` enum)
 *      2: u8 Type of flipping to perform (`Flip` enum)
 *      3: u8 Context (`Context` enum)
 *  Outputs:
 *      0: 0x001D0040
 *      1: ResultCode
 */
void FlipImage(Service::Interface* self);

/**
 * SetDetailSize service function
 *  Inputs:
 *      0: 0x001E0200
 *      1: u8 Camera select (`CameraSelect` enum)
 *      2: s16 Width
 *      3: s16 Height
 *      4: s16 Crop X0
 *      5: s16 Crop Y0
 *      6: s16 Crop X1
 *      7: s16 Crop Y1
 *      8: u8 Context (`Context` enum)
 *  Outputs:
 *      0: 0x001E0040
 *      1: ResultCode
 */
void SetDetailSize(Service::Interface* self);

/**
 * SetSize service function
 *  Inputs:
 *      0: 0x001F00C0
 *      1: u8 Camera select (`CameraSelect` enum)
 *      2: u8 Camera frame resolution (`Size` enum)
 *      3: u8 Context id (`Context` enum)
 *  Outputs:
 *      0: 0x001F0040
 *      1: ResultCode
 */
void SetSize(Service::Interface* self);

/**
 * SetFrameRate service function
 *  Inputs:
 *      0: 0x00200080
 *      1: u8 Camera select (`CameraSelect` enum)
 *      2: u8 Camera framerate (`FrameRate` enum)
 *  Outputs:
 *      0: 0x00200040
 *      1: ResultCode
 */
void SetFrameRate(Service::Interface* self);

/**
 * Returns calibration data relating the outside cameras to eachother, for use in AR applications.
 *
 *  Inputs:
 *      0: 0x002B0000
 *  Outputs:
 *      0: 0x002B0440
 *      1: ResultCode
 *      2-17: `StereoCameraCalibrationData` structure with calibration values
 */
void GetStereoCameraCalibrationData(Service::Interface* self);

/**
 * GetSuitableY2rStandardCoefficient service function
 *  Inputs:
 *      0: 0x00360000
 *  Outputs:
 *      0: 0x00360080
 *      1: ResultCode
 *      2: `StandartCoefficient` enum
 */
void GetSuitableY2rStandardCoefficient(Service::Interface* self);

/**
 * PlayShutterSound service function
 *  Inputs:
 *      0: 0x00380040
 *      1: u8 Sound ID
 *  Outputs:
 *      0: 0x00380040
 *      1: ResultCode
 */
void PlayShutterSound(Service::Interface* self);

/**
 * Initializes the camera driver. Must be called before using other functions.
 *  Inputs:
 *      0: 0x00390000
 *  Outputs:
 *      0: 0x00390040
 *      1: ResultCode
 */
void DriverInitialize(Service::Interface* self);

/**
 * Shuts down the camera driver.
 *  Inputs:
 *      0: 0x003A0000
 *  Outputs:
 *      0: 0x003A0040
 *      1: ResultCode
 */
void DriverFinalize(Service::Interface* self);

/// Initialize CAM service(s)
void Init();

/// Shutdown CAM service(s)
void Shutdown();

} // namespace CAM
} // namespace Service
