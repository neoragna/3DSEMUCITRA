// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <string>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"

#include "core/hle/applets/swkbd.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/gsp_gpu.h"
#include "core/hle/result.h"
#include "core/memory.h"

#include "video_core/video_core.h"

#include <iostream>

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {
namespace Applets {

ResultCode SoftwareKeyboard::ReceiveParameter(Service::APT::MessageParameter const& parameter) {
    if (parameter.signal != static_cast<u32>(Service::APT::SignalType::LibAppJustStarted)) {
        LOG_ERROR(Service_APT, "unsupported signal %u", parameter.signal);
        UNIMPLEMENTED();
        // TODO(Subv): Find the right error code
        return ResultCode(-1);
    }

    // The LibAppJustStarted message contains a buffer with the size of the framebuffer shared memory.
    // Create the SharedMemory that will hold the framebuffer data
    Service::APT::CaptureBufferInfo capture_info;
    ASSERT(sizeof(capture_info) == parameter.buffer.size());

    memcpy(&capture_info, parameter.buffer.data(), sizeof(capture_info));

    using Kernel::MemoryPermission;
    // Allocate a heap block of the required size for this applet.
    heap_memory = std::make_shared<std::vector<u8>>(capture_info.size);
    // Create a SharedMemory that directly points to this heap block.
    framebuffer_memory = Kernel::SharedMemory::CreateForApplet(heap_memory, 0, heap_memory->size(),
                                                               MemoryPermission::ReadWrite, MemoryPermission::ReadWrite,
                                                               "SoftwareKeyboard Memory");

    // Send the response message with the newly created SharedMemory
    Service::APT::MessageParameter result;
    result.signal = static_cast<u32>(Service::APT::SignalType::LibAppFinished);
    result.buffer.clear();
    result.destination_id = static_cast<u32>(Service::APT::AppletId::Application);
    result.sender_id = static_cast<u32>(id);
    result.object = framebuffer_memory;

    Service::APT::SendParameter(result);
    return RESULT_SUCCESS;
}

ResultCode SoftwareKeyboard::StartImpl(Service::APT::AppletStartupParameter const& parameter) {
    ASSERT_MSG(parameter.buffer.size() == sizeof(config), "The size of the parameter (SoftwareKeyboardConfig) is wrong");

    memcpy(&config, parameter.buffer.data(), parameter.buffer.size());
    text_memory = boost::static_pointer_cast<Kernel::SharedMemory, Kernel::Object>(parameter.object);

    // TODO(Subv): Verify if this is the correct behavior
    memset(text_memory->GetPointer(), 0, text_memory->size);

    DrawScreenKeyboard();

    started = true;
    return RESULT_SUCCESS;
}

void SoftwareKeyboard::Update() {
    // TODO(Subv): Handle input using the touch events from the HID module

    // TODO(Subv): Remove this hardcoded text
	std::string ctext;
	std::getline(std::cin, ctext, '\n');
	std::u16string text = Common::UTF8ToUTF16(ctext);
    memcpy(text_memory->GetPointer(), text.c_str(), text.length() * sizeof(char16_t));

    // TODO(Subv): Ask for input and write it to the shared memory
    config.return_code = SwkbdResult::SWKBD_D1_CLICK1;
	config.text_length = text.length();
    config.text_offset = 0;

    // TODO(Subv): We're finalizing the applet immediately after it's started,
    // but we should defer this call until after all the input has been collected.
    Finalize();
}

void SoftwareKeyboard::DrawScreenKeyboard() {
    auto bottom_screen = GSP_GPU::GetFrameBufferInfo(0, 1);
    auto info = bottom_screen->framebuffer_info[bottom_screen->index];

    // TODO(Subv): Draw the HLE keyboard, for now just zero-fill the framebuffer
    Memory::ZeroBlock(info.address_left, info.stride * 320);

    GSP_GPU::SetBufferSwap(1, info);
}

void SoftwareKeyboard::Finalize() {
    // Let the application know that we're closing
    Service::APT::MessageParameter message;
    message.buffer.resize(sizeof(SoftwareKeyboardConfig));
    std::memcpy(message.buffer.data(), &config, message.buffer.size());
    message.signal = static_cast<u32>(Service::APT::SignalType::LibAppClosed);
    message.destination_id = static_cast<u32>(Service::APT::AppletId::Application);
    message.sender_id = static_cast<u32>(id);
    Service::APT::SendParameter(message);

    started = false;
}

}
} // namespace
