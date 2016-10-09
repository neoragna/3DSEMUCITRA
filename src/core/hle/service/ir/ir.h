// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Service {

class Interface;

namespace IR {

enum class ConnectionStatus : u8 {
    STOPPED,
    TRYING_TO_CONNECT,
    CONNECTED,
    DISCONNECTING,
    FATAL_ERROR
};

enum class ConnectionRole : u8 { NONE, REQUIRE, WAIT };

enum class BaudRate {
    BAUDRATE_115200 = 3,
    BAUDRATE_96000 = 4,
    BAUDRATE_72000 = 5,
    BAUDRATE_48000 = 6,
    BAUDRATE_36000 = 7,
    BAUDRATE_24000 = 8,
    BAUDRATE_18000 = 9,
    BAUDRATE_12000 = 10,
    BAUDRATE_9600 = 11,
    BAUDRATE_6000 = 12,
    BAUDRATE_3000 = 13,
    BAUDRATE_57600 = 14,
    BAUDRATE_38400 = 15,
    BAUDRATE_19200 = 16,
    BAUDRATE_7200 = 17,
    BAUDRATE_4800 = 18
};

struct ConnectionInfo {
    u8 unk_00;                          // 0x00
    u8 unk_01;                          // 0x01
    u8 unk_02;                          // 0x02
    u8 unk_03;                          // 0x03
    u8 unk_04;                          // 0x04
    u8 unk_05;                          // 0x05
    u8 unk_06;                          // 0x06
    u8 unk_07;                          // 0x07
    ConnectionStatus connection_status; // 0x08
    u8 unk_09;                          // 0x09
    ConnectionRole connection_role;     // 0x0A
    u8 unk_0B;                          // 0x0B
    u8 unk_0C;                          // 0x0C
    u8 unk_0D;                          // 0x0D
    bool unk_0E;                        // 0x0E, must be true
    u8 unk_0F;                          // 0x0F
};

struct StructHead {
    unsigned int position;
    unsigned int size;
};

struct TransInfo {
    unsigned int ReadIdx;
    unsigned int WriteIdx;
    unsigned long long Count;
};

// TODO make all structures flat
struct TransferMemory {             // extraPadWorkingMemory
    ConnectionInfo connection_info; // 0x00-0x0F
    TransInfo trans_info;           // 0x10
    StructHead struct_head;         // 0x20
    u8 head[0x4A];                  // 0x28 size = 0x4A
};

/**
 * IR::GetHandles service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Translate header, used by the ARM11-kernel
 *      3 : Shared memory handle
 *      4 : Event handle
 */
void GetHandles(Interface* self);

/**
 * IR::InitializeIrNopShared service function
 *  Inputs:
 *      1 : Size of transfer buffer
 *      2 : Recv buffer size
 *      3 : unknown
 *      4 : Send buffer size
 *      5 : unknown
 *      6 : BaudRate (u8)
 *      7 : 0
 *      8 : Handle of transfer shared memory
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void InitializeIrNopShared(Interface* self);

/**
 * IR::FinalizeIrNop service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void FinalizeIrNop(Interface* self);

/**
 * IR::GetConnectionStatusEvent service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Connection Status Event handle
 */
void GetConnectionStatusEvent(Interface* self);

/* IR::SendIrNop service function
 *  Inputs:
 *      1 : Size
 *      2 : (Size << 14 | 2)
 *      3 : Buffer
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
 
void SendIrNop(Interface* self);
void SendIrNopLarge(Interface* self);
void ReceiveIrnop(Interface* self);
void ReceiveIrnopLarge(Interface* self);
void GetConnectionStatus(Interface* self);
void GetSendEvent(Interface* self);
void GetReceiveEvent(Interface* self);

/**
 * IR::Disconnect service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void Disconnect(Interface* self);

/**
 * IR::RequireConnection service function
 *  Inputs:
 *      1 : unknown (u8), looks like always 1
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void RequireConnection(Interface* self);

/**
* IR::WaitConnection service function
*  Inputs:
*      1 : unknown (u8)
*      2-3 : Timeout
*  Outputs:
*      1 : Result of function, 0 on success, otherwise error code
*/
void WaitConnection(Interface* self);

void AutoConnection(Interface* self);

/**
 * IR::GetSendEvent service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      3 : Connection Status Event handle
 */

/**
 * IR::GetReceiveEvent service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      3 : Receive Status Event handle
 */

void ReleaseReceivedData(Interface* self);

void GetConnectionRole(Interface* self);

void ClearReceiveBuffer(Interface* self);

void ClearSendBuffer(Interface* self);

/// Initialize IR service
void Init();

/// Shutdown IR service
void Shutdown();

} // namespace IR
} // namespace Service
