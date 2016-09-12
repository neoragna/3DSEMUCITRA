// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/session.h"
#include "core/hle/kernel/thread.h"

namespace IPC {

bool CheckBufferMappingTranslation(MappedBufferPermissions mapping_type, u32 size, u32 translation) {
    if (0x8 == translation) {
        return false;
    }
    switch (mapping_type) {
    case IPC::MappedBufferPermissions::R:
        if (((size << 4) | 0xA) == translation) {
            return true;
        }
        break;
    case IPC::MappedBufferPermissions::W:
        if (((size << 4) | 0xC) == translation) {
            return true;
        }
        break;
    case IPC::MappedBufferPermissions::RW:
        if (((size << 4) | 0xE) == translation) {
            return true;
        }
        break;
    }
    return false;
}

} // namespace IPC

namespace Kernel {

Session::Session() {}
Session::~Session() {}

}
