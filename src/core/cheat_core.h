// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <memory>
#include <vector>

#include "common/string_util.h"
#include "core/core_timing.h"

namespace CheatCore {
/*
 * Starting point for running cheat codes. Initializes tick event and executes cheats every tick.
 */
void Init();
void Shutdown();
void RefreshCheats();
}
namespace CheatEngine {
/*
 * Represents a single line of a cheat, i.e. 1xxxxxxxx yyyyyyyy
 */
struct CheatLine {
    explicit CheatLine(std::string line) {
        line = std::string(line.c_str()); // remove '/0' characters if any.
        line = Common::Trim(line);
        constexpr int cheat_length = 17;
        if (line.length() != cheat_length) {
            type = -1;
            cheat_line = line;
            return;
        }
        try {
            type = std::stoi(line.substr(0, 1), 0, 16);
            if (type == 0xD) // 0xD types have extra subtype value, i.e. 0xDA
                sub_type = std::stoi(line.substr(1, 1), 0, 16);
            address = std::stoi(line.substr(1, 8), 0, 16);
            value = std::stoi(line.substr(10, 8), 0, 16);
            cheat_line = line;
        } catch (std::exception e) {
            type = -1;
            cheat_line = line;
            return;
        }
    }
    int type;
    int sub_type;
    u32 address;
    u32 value;
    std::string cheat_line;
};

/*
 * Base Interface for all types of cheats.
 */
class CheatBase {
public:
    virtual void Execute() = 0;
    virtual ~CheatBase() = default;
    virtual std::string ToString() = 0;
    std::vector<std::string>& GetNotes() {
        return notes;
    }
    void SetNotes(std::vector<std::string> val) {
        notes = val;
    }
    bool& GetEnabled() {
        return enabled;
    }
    void SetEnabled(bool val) {
        enabled = val;
    }
    std::string& GetType() {
        return type;
    }
    void SetType(std::string val) {
        type = val;
    }
    std::vector<CheatLine>& GetCheatLines() {
        return cheat_lines;
    }
    void SetCheatLines(std::vector<CheatLine> val) {
        cheat_lines = val;
    }
    std::string& GetName() {
        return name;
    }
    void SetName(std::string val) {
        name = val;
    }

protected:
    std::vector<std::string> notes;
    bool enabled;
    std::string type;
    std::vector<CheatLine> cheat_lines;
    std::string name;
};
/*
 * Implements support for Gateway (GateShark) cheats.
 */
class GatewayCheat : public CheatBase {
public:
    GatewayCheat(std::string name_) {
        cheat_lines = std::vector<CheatLine>();
        notes = std::vector<std::string>();
        enabled = false;
        name = name_;
        type = "Gateway";
    };
    GatewayCheat(std::vector<CheatLine> cheat_lines_, std::vector<std::string> notes_,
                 bool enabled_, std::string name_) {
        cheat_lines = std::move(cheat_lines_);
        notes = std::move(notes_);
        enabled = enabled_;
        name = std::move(name_);
        type = "Gateway";
    };
    void Execute() override;
    std::string ToString() override;
};

/*
 * Handles loading/saving of cheats and executing them.
 */
class CheatEngine {
public:
    CheatEngine();
    void Run();
    static std::vector<std::shared_ptr<CheatBase>> ReadFileContents();
    static void Save(std::vector<std::shared_ptr<CheatBase>> cheats);
    void RefreshCheats();

private:
    std::vector<std::shared_ptr<CheatBase>> cheats_list;
    static std::string GetFilePath();
};
}
