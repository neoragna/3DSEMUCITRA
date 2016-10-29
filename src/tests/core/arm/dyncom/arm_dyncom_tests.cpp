// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch.hpp>

#include "core/arm/dyncom/arm_dyncom.h"
#include "tests/core/arm/arm_test_common.h"

namespace ArmTests {

constexpr u32 CpsrTFlag = 1 << 5;

TEST_CASE("ARM_DynCom (thumb): revsh", "[arm_dyncom]") {
    TestEnvironment test_env(false);
    test_env.SetMemory16(0, 0xBADC); // revsh r4, r3
    test_env.SetMemory16(2, 0xE7FE); // b +#0

    ARM_DynCom dyncom(USER32MODE);
    dyncom.SetPC(0);
    dyncom.SetCPSR(dyncom.GetCPSR() | CpsrTFlag);

    const u32 initial_cpsr = dyncom.GetCPSR();

    SECTION("Section 1") {
        dyncom.SetReg(3, 0x12345678);

        dyncom.ExecuteInstructions(1);

        REQUIRE(dyncom.GetReg(3) == 0x12345678);
        REQUIRE(dyncom.GetReg(4) == 0x7856);
        REQUIRE(dyncom.GetPC() == 2);
        REQUIRE(dyncom.GetCPSR() == initial_cpsr);
        REQUIRE(test_env.GetWriteRecords().empty());
    }

    SECTION("Section 2") {
        dyncom.SetReg(3, 0);

        dyncom.ExecuteInstructions(1);

        REQUIRE(dyncom.GetReg(4) == 0);
        REQUIRE(dyncom.GetReg(3) == 0);
        REQUIRE(dyncom.GetPC() == 2);
        REQUIRE(dyncom.GetCPSR() == initial_cpsr);
        REQUIRE(test_env.GetWriteRecords().empty());
    }

    SECTION("Section 3") {
        dyncom.SetReg(3, 0x80);

        dyncom.ExecuteInstructions(1);

        REQUIRE(dyncom.GetReg(3) == 0x80);
        REQUIRE(dyncom.GetReg(4) == 0xFFFF8000);
        REQUIRE(dyncom.GetPC() == 2);
        REQUIRE(dyncom.GetCPSR() == initial_cpsr);
        REQUIRE(test_env.GetWriteRecords().empty());
    }
}

} // namespace ArmTests
