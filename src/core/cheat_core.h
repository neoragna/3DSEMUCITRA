// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <memory>
#include <vector>

#include "common/string_util.h"
#include "core/core_timing.h"
/*
 * Starting point for running cheat codes. Initializes tick event and executes cheats every tick.
 */
namespace CheatCore {
    void Init();
    void Shutdown();
    void RefreshCheats();
}
namespace CheatEngine {
    /*
    * Represents a single line of a cheat, i.e. 1xxxxxxxx yyyyyyyy
    */
    struct CheatLine {
        CheatLine(std::string line) {
            line = std::string(line.c_str()); // remove '/0' characters if any.
            line = Common::Trim(line);
            if (line.length() != 17) {
                type = -1;
                cheat_line = line;
                return;
            }
            try {
                type = stoi(line.substr(0, 1), 0, 16);
                if (type == 0xD)
                    sub_type = stoi(line.substr(1, 1), 0, 16);
                address = stoi(line.substr(1, 8), 0, 16);
                value = stoi(line.substr(10, 8), 0, 16);
                cheat_line = line;
            }
            catch (std::exception e) {
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
    class ICheat {
    public:
        virtual void Execute() = 0;
        virtual ~ICheat() = default;
        virtual std::string ToString() = 0;

        std::vector<std::string> notes;
        bool enabled;
        std::string type;
        std::vector<CheatLine> cheat_lines;
        std::string name;
    };
    /*
     * Implements support for Gateway (GateShark) cheats.
     */
    class GatewayCheat : public ICheat {
    public:
        GatewayCheat(std::vector<CheatLine> _cheatlines, std::vector<std::string> _notes, bool _enabled, std::string _name) {
            cheat_lines = _cheatlines;
            notes = _notes;
            enabled = _enabled;
            name = _name;
            type = "Gateway";
        };
        void Execute() override;
        std::string ToString() override;
    private:
    };

    /*
    * Handles loading/saving of cheats and executing them.
    */
    class CheatEngine {
    public:
        CheatEngine();
        void Run();
        static std::vector<std::shared_ptr<ICheat>> ReadFileContents();
        static void Save(std::vector<std::shared_ptr<ICheat>> cheats);
    private:
        std::vector<std::shared_ptr<ICheat>> cheats_list;
    };
}
