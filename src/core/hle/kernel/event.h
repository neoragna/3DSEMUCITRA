// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"

namespace Kernel {

class Event final : public WaitObject {
public:
    /**
     * Creates an event
     * @param reset_type ResetType describing how to create event
     * @param name Optional name of event
     */
    static SharedPtr<Event> Create(ResetType reset_type, std::string name = "Unknown");

    std::string GetTypeName() const override { return "Event"; }
    std::string GetName() const override { return name; }

    static const HandleType HANDLE_TYPE = HandleType::Event;
    HandleType GetHandleType() const override { return HANDLE_TYPE; }

    ResetType reset_type;                   ///< Current ResetType

    bool signaled;                          ///< Whether the event has already been signaled
    std::string name;                       ///< Name of event (optional)

    bool ShouldWait() override;
    void Acquire() override;

    void Signal();
    void Clear();
    void ReSignal() { signaled = true; re_signal = true; }
private:
    bool re_signal = false;
    Event();
    ~Event() override;
};

} // namespace
