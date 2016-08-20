// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/service/mic_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace MIC_U

namespace MIC_U {

/**
 * MIC_U::SetClientVersion service function
 *  Inputs:
 *      1 : Used SDK Version
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetClientVersion(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const u32 version = cmd_buff[1];
    self->SetVersion(version);

    LOG_WARNING(Service_MIC, "(STUBBED) called, version: 0x%08X", version);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
}


const Interface::FunctionInfo FunctionTable[] = {
    {0x00010042, nullptr,               "MapSharedMem"},
    {0x00020000, nullptr,               "UnmapSharedMem"},
    {0x00030140, nullptr,               "Initialize"},
    {0x00040040, nullptr,               "AdjustSampling"},
    {0x00050000, nullptr,               "StopSampling"},
    {0x00060000, nullptr,               "IsSampling"},
    {0x00070000, nullptr,               "GetEventHandle"},
    {0x00080040, nullptr,               "SetGain"},
    {0x00090000, nullptr,               "GetGain"},
    {0x000A0040, nullptr,               "SetPower"},
    {0x000B0000, nullptr,               "GetPower"},
    {0x000C0042, nullptr,               "size"},
    {0x000D0040, nullptr,               "SetClamp"},
    {0x000E0000, nullptr,               "GetClamp"},
    {0x000F0040, nullptr,               "SetAllowShellClosed"},
    {0x00100040, SetClientVersion,      "SetClientVersion"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
