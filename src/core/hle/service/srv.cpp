// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"

#include "core/hle/service/srv.h"
#include "core/hle/kernel/event.h"

#include "core/hle/service/service_wrapper.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SRV

namespace SRV {

static Kernel::SharedPtr<Kernel::Event> event_handle;

static void Initialize(const IPC::CallingPidParam& pid) {
    IPC::Return(RESULT_SUCCESS.raw);
}

static void GetProcSemaphore() {
    // TODO(bunnei): Change to a semaphore once these have been implemented
    event_handle = Kernel::Event::Create(Kernel::ResetType::OneShot, "SRV:Event");
    event_handle->Clear();

    IPC::Return(RESULT_SUCCESS.raw, IPC::HandleParam{false, {Kernel::g_handle_table.Create(event_handle).MoveFrom()}});
}

static void GetServiceHandle(const char(& name)[8], u32 name_length, u32 flags) {
    std::string port_name = std::string(name, 0, name_length);
    auto it = Service::g_srv_services.find(port_name);

    if (it != Service::g_srv_services.end()) {
        auto handle = Kernel::g_handle_table.Create(it->second).MoveFrom();
        IPC::Return(RESULT_SUCCESS.raw, IPC::HandleParam{false, {handle}});
        LOG_TRACE(Service_SRV, "called port=%s, handle=0x%08X", port_name.c_str(), handle);
    } else {
        LOG_ERROR(Service_SRV, "(UNIMPLEMENTED) called port=%s", port_name.c_str());
        IPC::Return(UnimplementedFunction(ErrorModule::SRV).raw);
    }
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010002, IPC::Wrap<decltype(Initialize)>::F<Initialize>, "Initialize"},
    {0x00020000, IPC::Wrap<decltype(GetProcSemaphore)>::F<GetProcSemaphore>, "GetProcSemaphore"},
    {0x00030100, nullptr,             "RegisterService"},
    {0x000400C0, nullptr,             "UnregisterService"},
    {0x00050100, IPC::Wrap<decltype(GetServiceHandle)>::F<GetServiceHandle>, "GetServiceHandle"},
    {0x000600C2, nullptr,             "RegisterHandle"},
    {0x00090040, nullptr,             "Subscribe"},
    {0x000B0000, nullptr,             "ReceiveNotification"},
    {0x000C0080, nullptr,             "PublishToSubscriber"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

Interface::~Interface() {
    event_handle = nullptr;
}

} // namespace
