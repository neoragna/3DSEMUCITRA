// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "core/hle/config_mem.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ConfigMem {

ConfigMemDef config_mem;

void Init() {
    std::memset(&config_mem, 0, sizeof(config_mem));

    config_mem.kernel_unk = 0;
    config_mem.kernel_version_rev = 0;
    config_mem.kernel_version_min = 0x10;
    config_mem.kernel_version_maj = 0x0;
    config_mem.sys_core_ver = 0x34;
    config_mem.kernel_ctr_sdk_ver = 0x33DD8;
    config_mem.update_flag = 0; // No update
    config_mem.unit_info = 0x1; // Bit 0 set for Retail
    config_mem.prev_firm = 0;
	//The Firm data base on FIRM ver.11.3
    config_mem.firm_unk = 0;
    config_mem.firm_version_rev = 0;
    config_mem.firm_version_min = 0x10;
    config_mem.firm_version_maj = 0;
    config_mem.firm_sys_core_ver = 0x34;
    config_mem.firm_ctr_sdk_ver = 0x33DD8;
}

} // namespace
