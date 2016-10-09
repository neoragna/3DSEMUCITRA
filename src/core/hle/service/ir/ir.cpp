// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/string_util.h"

#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/hle/service/ir/ir_u.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/service.h"

namespace Service {
namespace IR {

static Kernel::SharedPtr<Kernel::Event> handle_event;
static Kernel::SharedPtr<Kernel::Event> send_event;
static Kernel::SharedPtr<Kernel::Event> receive_event;
static Kernel::SharedPtr<Kernel::Event> conn_status_event;
static Kernel::SharedPtr<Kernel::SharedMemory> shared_memory;
static Kernel::SharedPtr<Kernel::SharedMemory> transfer_shared_memory;

static u32 transfer_buff_size;
static u32 recv_buff_size;
static u32 unk1;
static u32 send_buff_size;
static u32 unk2;
static BaudRate baud_rate;

static bool is_confirmed;

static ConnectionStatus connection_status = ConnectionStatus::STOPPED;
static ConnectionRole connection_role;

void GetHandles(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0x4000000;
    cmd_buff[3] = Kernel::g_handle_table.Create(Service::IR::shared_memory).MoveFrom();
    cmd_buff[4] = Kernel::g_handle_table.Create(Service::IR::handle_event).MoveFrom();
}

void InitializeIrNopShared(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    transfer_buff_size = cmd_buff[1];
    recv_buff_size = cmd_buff[2];
    unk1 = cmd_buff[3];
    send_buff_size = cmd_buff[4];
    unk2 = cmd_buff[5];
    baud_rate = static_cast<BaudRate>(cmd_buff[6] & 0xFF);
    Handle handle = cmd_buff[8];

    if (Kernel::g_handle_table.IsValid(handle)) {
        transfer_shared_memory = Kernel::g_handle_table.Get<Kernel::SharedMemory>(handle);
        transfer_shared_memory->name = "IR:TransferSharedMemory";
    } else {
        LOG_ERROR(Service_IR, "Error handle for shared memory");
        cmd_buff[1] = -1;
        return;
    }

    connection_status = ConnectionStatus::STOPPED;
    connection_role = ConnectionRole::NONE;

    is_confirmed = false;

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called, transfer_buff_size=%d, recv_buff_size=%d, "
                            "unk1=%d, send_buff_size=%d, unk2=%d, baud_rate=%u, handle=0x%08X",
                transfer_buff_size, recv_buff_size, unk1, send_buff_size, unk2, baud_rate, handle);
}

void FillConnectionInfo() {
    TransferMemory& trans_mem =
        *reinterpret_cast<TransferMemory*>(transfer_shared_memory->GetPointer());
    trans_mem.connection_info.connection_status = connection_status;
    trans_mem.connection_info.connection_role = connection_role;
    trans_mem.connection_info.unk_0E = true;
}

void RequireConnection(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    connection_status = ConnectionStatus::CONNECTED;
    connection_role = ConnectionRole::REQUIRE;
    is_confirmed = false;

    FillConnectionInfo();

    conn_status_event->Signal(); // TODO really need this?

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void WaitConnection(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 unk = cmd_buff[1];
    uint64_t timeout = (uint64_t)cmd_buff[3] << 32 | cmd_buff[2];

    connection_status = ConnectionStatus::CONNECTED;
    connection_role = ConnectionRole::WAIT;
    is_confirmed = false;

    FillConnectionInfo();

    conn_status_event->Signal(); // TODO really need this?

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called, unk=%u, timeout=%ull", unk, timeout);
}

void AutoConnection(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    uint64_t sendReplyDelay = (uint64_t)cmd_buff[2] << 32 | cmd_buff[1];
    uint64_t waitRequestMin = (uint64_t)cmd_buff[4] << 32 | cmd_buff[3];
    uint64_t waitRequestMax = (uint64_t)cmd_buff[6] << 32 | cmd_buff[5];
    uint64_t waitReplyMin = (uint64_t)cmd_buff[8] << 32 | cmd_buff[7];
    uint64_t waitReplyMax = (uint64_t)cmd_buff[10] << 32 | cmd_buff[9];

    // TODO: check which needed for CCP

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void Disconnect(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    connection_status = ConnectionStatus::STOPPED;
    connection_role = ConnectionRole::NONE;
    is_confirmed = false;

    FillConnectionInfo();

    conn_status_event->Signal(); // TODO really need this?

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void GetConnectionStatus(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    cmd_buff[2] = static_cast<u32>(connection_status); // TODO: check index

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void GetConnectionRole(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    cmd_buff[2] = static_cast<u32>(connection_role);

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void GetConnectionStatusEvent(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[3] = Kernel::g_handle_table.Create(Service::IR::conn_status_event).MoveFrom();

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void GetSendEvent(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[3] = Kernel::g_handle_table.Create(Service::IR::send_event).MoveFrom();

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void GetReceiveEvent(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[3] = Kernel::g_handle_table.Create(Service::IR::receive_event).MoveFrom();

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void FinalizeIrNop(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void ReleaseReceivedData(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 addr = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called, addr=0x%08X", addr);
}

void ClearReceiveBuffer(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void ClearSendBuffer(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

static ResultCode ProcessPacket(bool send, VAddr addr, u32 size) {
    u8 command_id = Memory::Read8(addr);
    switch (command_id) {
    case 2: { // request read
        u8 unk5 = Memory::Read8(addr + 1);
        u16 unk6 = Memory::Read16(addr + 2);
        u16 unk8 = Memory::Read16(addr + 4);
        // receive_event->Signal();
        break;
    }
    }
    return RESULT_SUCCESS;
}

void SendIrNop(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = cmd_buff[1];
    u32 buffer_addr = cmd_buff[3];
    /*
    if(!is_confirmed) {

        ASSERT(Memory::Read8(buffer_addr) == 'I');
        ASSERT(Memory::Read8(buffer_addr + 1) == 'C');
        ASSERT(Memory::Read8(buffer_addr + 2) == 'S');

        receive_event->ReSignal();

        is_confirmed = true;
        cmd_buff[1] = RESULT_SUCCESS.raw;
    }
    else {
        cmd_buff[1] = ProcessPacket(true, buffer_addr, size).raw;
    }*/

    cmd_buff[1] = ProcessPacket(true, buffer_addr, size).raw;
    receive_event->ReSignal();

    LOG_WARNING(Service_IR, "(STUBBED) called, addr=0x%08X, size=%d", buffer_addr, size);
    Common::Dump(buffer_addr, size);
}

void SendIrNopLarge(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = cmd_buff[1];
    u32 buffer_addr = cmd_buff[3];

    cmd_buff[1] = ProcessPacket(true, buffer_addr, size).raw;

    LOG_WARNING(Service_IR, "(STUBBED) called, addr=0x%08X, size=%d", buffer_addr, size);
    Common::Dump(buffer_addr, size);
}

void ReceiveIrnop(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = cmd_buff[1];
    u32 size_shift = cmd_buff[2];
    u32 buffer_addr = cmd_buff[3];

    receive_event->Signal();

    cmd_buff[1] = ProcessPacket(false, buffer_addr, size).raw;

    LOG_WARNING(Service_IR, "(STUBBED) called, addr=0x%08X, size=%d", buffer_addr, size);
    Common::Dump(buffer_addr, size);
}

void ReceiveIrnopLarge(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = cmd_buff[1];
    u32 size_shift = cmd_buff[2];
    u32 buffer_addr = cmd_buff[3];

    cmd_buff[1] = ProcessPacket(false, buffer_addr, size).raw;

    LOG_WARNING(Service_IR, "(STUBBED) called, addr=0x%08X, size=%d", buffer_addr, size);
    Common::Dump(buffer_addr, size);
}

void Init() {
    using namespace Kernel;

    AddService(new IR_RST_Interface);
    AddService(new IR_U_Interface);
    AddService(new IR_User_Interface);

    using Kernel::MemoryPermission;
    shared_memory = SharedMemory::Create(nullptr, 0x1000, Kernel::MemoryPermission::ReadWrite,
                                         Kernel::MemoryPermission::ReadWrite, 0,
                                         Kernel::MemoryRegion::BASE, "IR:SharedMemory");
    transfer_shared_memory = nullptr;

    // Create event handle(s)
    handle_event = Event::Create(ResetType::OneShot, "IR:HandleEvent");
    conn_status_event = Event::Create(ResetType::OneShot, "IR:ConnectionStatusEvent");
    send_event = Event::Create(ResetType::OneShot, "IR:SendEvent");
    receive_event = Event::Create(ResetType::OneShot, "IR:ReceiveEvent");

    connection_status = ConnectionStatus::STOPPED;
    connection_role = ConnectionRole::NONE;

    is_confirmed = false;
}

void Shutdown() {
    transfer_shared_memory = nullptr;
    shared_memory = nullptr;
    handle_event = nullptr;
    conn_status_event = nullptr;
    send_event = nullptr;
    receive_event = nullptr;
}

} // namespace IR

} // namespace Service
