// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ir/ir_user.h"

namespace Service {
namespace IR {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010182, nullptr, "InitializeIrNop"},
    {0x00020000, FinalizeIrNop, "FinalizeIrNop"},
    {0x00030000, ClearReceiveBuffer, "ClearReceiveBuffer"},
    {0x00040000, ClearSendBuffer, "ClearSendBuffer"},
    {0x000500C0, WaitConnection, "WaitConnection"},
    {0x00060040, RequireConnection, "RequireConnection"},
    {0x000702C0, AutoConnection, "AutoConnection"},
    {0x00080000, nullptr, "AnyConnection"},
    {0x00090000, Disconnect, "Disconnect"},
    {0x000A0000, GetReceiveEvent, "GetReceiveEvent"},
    {0x000B0000, GetSendEvent, "GetSendEvent"},
    {0x000C0000, GetConnectionStatusEvent, "GetConnectionStatusEvent"},
    {0x000D0042, SendIrNop, "SendIrNop"},
    {0x000E0042, SendIrNopLarge, "SendIrNopLarge"},
    {0x000F0040, ReceiveIrnop, "ReceiveIrnop"},
    {0x00100042, ReceiveIrnopLarge, "ReceiveIrnopLarge"},
    {0x00110040, nullptr, "GetLatestReceiveErrorResult"},
    {0x00120040, nullptr, "GetLatestSendErrorResult"},
    {0x00130000, GetConnectionStatus, "GetConnectionStatus"},
    {0x00140000, nullptr, "GetTryingToConnectStatus"},
    {0x00150000, nullptr, "GetReceiveSizeFreeAndUsed"},
    {0x00160000, nullptr, "GetSendSizeFreeAndUsed"},
    {0x00170000, GetConnectionRole, "GetConnectionRole"},
    {0x00180182, InitializeIrNopShared, "InitializeIrNopShared"},
    {0x00190040, ReleaseReceivedData, "ReleaseReceivedData"},
    {0x001A0040, nullptr, "SetOwnMachineId"},
};

IR_User_Interface::IR_User_Interface() {
    Register(FunctionTable);
}

} // namespace IR
} // namespace Service
