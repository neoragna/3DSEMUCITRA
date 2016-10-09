// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/common_funcs.h"

#include "core/hle/applets/applet.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hle/service/apt/apt.h"

namespace HLE {
namespace Applets {

#define SWKBD_MAX_BUTTON           3
/// Maximum button text length, in UTF-16 code units.
#define SWKBD_MAX_BUTTON_TEXT_LEN  16
/// Maximum hint text length, in UTF-16 code units.
#define SWKBD_MAX_HINT_TEXT_LEN    64
/// Maximum filter callback error message length, in UTF-16 code units.
#define SWKBD_MAX_CALLBACK_MSG_LEN 256

/// Keyboard types
enum class SwkbdType : u32 {
    SWKBD_TYPE_NORMAL = 0, ///< Normal keyboard with several pages (QWERTY/accents/symbol/mobile)
    SWKBD_TYPE_QWERTY,     ///< QWERTY keyboard only.
    SWKBD_TYPE_NUMPAD,     ///< Number pad.
    SWKBD_TYPE_WESTERN,    ///< On JPN systems, a text keyboard without Japanese input capabilities, otherwise same as SWKBD_TYPE_NORMAL.
};

/// Keyboard dialog buttons.
enum class SwkbdButton : u32 {
    SWKBD_BUTTON_LEFT = 0, ///< Left button (usually Cancel)
    SWKBD_BUTTON_MIDDLE,   ///< Middle button (usually I Forgot)
    SWKBD_BUTTON_RIGHT,    ///< Right button (usually OK)
    SWKBD_BUTTON_CONFIRM = SWKBD_BUTTON_RIGHT,
    SWKBD_BUTTON_NONE,     ///< No button (returned by swkbdInputText in special cases)
};

/// Accepted input types.
enum class SwkbdValidInput : u32 {
    SWKBD_ANYTHING = 0,      ///< All inputs are accepted.
    SWKBD_NOTEMPTY,          ///< Empty inputs are not accepted.
    SWKBD_NOTEMPTY_NOTBLANK, ///< Empty or blank inputs (consisting solely of whitespace) are not accepted.
    SWKBD_NOTBLANK_NOTEMPTY = SWKBD_NOTEMPTY_NOTBLANK,
    SWKBD_NOTBLANK,          ///< Blank inputs (consisting solely of whitespace) are not accepted, but empty inputs are.
    SWKBD_FIXEDLEN,          ///< The input must have a fixed length (specified by maxTextLength in swkbdInit).
};

/// Keyboard password modes.
enum class SwkbdPasswordMode : u32 {
    SWKBD_PASSWORD_NONE = 0,   ///< Characters are not concealed.
    SWKBD_PASSWORD_HIDE,       ///< Characters are concealed immediately.
    SWKBD_PASSWORD_HIDE_DELAY, ///< Characters are concealed a second after they've been typed.
};

/// Keyboard return values.
enum class SwkbdResult : s32
{
    SWKBD_NONE = -1,    ///< Dummy/unused.
    SWKBD_INVALID_INPUT = -2, ///< Invalid parameters to swkbd.
    SWKBD_OUTOFMEM = -3, ///< Out of memory.

    SWKBD_D0_CLICK = 0, ///< The button was clicked in 1-button dialogs.
    SWKBD_D1_CLICK0,    ///< The left button was clicked in 2-button dialogs.
    SWKBD_D1_CLICK1,    ///< The right button was clicked in 2-button dialogs.
    SWKBD_D2_CLICK0,    ///< The left button was clicked in 3-button dialogs.
    SWKBD_D2_CLICK1,    ///< The middle button was clicked in 3-button dialogs.
    SWKBD_D2_CLICK2,    ///< The right button was clicked in 3-button dialogs.

    SWKBD_HOMEPRESSED = 10, ///< The HOME button was pressed.
    SWKBD_RESETPRESSED,     ///< The soft-reset key combination was pressed.
    SWKBD_POWERPRESSED,     ///< The POWER button was pressed.

    SWKBD_PARENTAL_OK = 20, ///< The parental PIN was verified successfully.
    SWKBD_PARENTAL_FAIL,    ///< The parental PIN was incorrect.

    SWKBD_BANNED_INPUT = 30, ///< The filter callback returned SWKBD_CALLBACK_CLOSE.
};

struct SoftwareKeyboardConfig {
    SwkbdType type;
    SwkbdButton num_buttons_m1;
    SwkbdValidInput valid_input;
    SwkbdPasswordMode password_mode;
    s32 is_parental_screen;
    s32 darken_top_screen;
    u32 filter_flags;
    u32 save_state_flags;
    u16 max_text_length; ///< Maximum length of the input text
    u16 dict_word_count;
    u16 max_digits;
    u16 button_text[SWKBD_MAX_BUTTON][SWKBD_MAX_BUTTON_TEXT_LEN + 1];
    u16 numpad_keys[2];
    u16 hint_text[SWKBD_MAX_HINT_TEXT_LEN + 1]; ///< Text to display when asking the user for input
    bool predictive_input;
    bool multiline;
    bool fixed_width;
    bool allow_home;
    bool allow_reset;
    bool allow_power;
    bool unknown; // XX: what is this supposed to do? "communicateWithOtherRegions"
    bool default_qwerty;
    bool button_submits_text[4];
    u16 language; // XX: not working? supposedly 0 = use system language, CFG_Language+1 = pick language

    u32 initial_text_offset; ///< Offset of the default text in the output SharedMemory
    u32 dict_offset;
    u32 initial_status_offset;
    u32 initial_learning_offset;
    u32 shared_memory_size; ///< Size of the SharedMemory
    u32 version;

    SwkbdResult return_code; ///< Return code of the SoftwareKeyboard, usually 2, other values are unknown

    u32 status_offset;
    u32 learning_offset;

    u32 text_offset; ///< Offset in the SharedMemory where the output text starts
    u16 text_length; ///< Length in characters of the output text

    int callback_result;
    u16 callback_msg[SWKBD_MAX_CALLBACK_MSG_LEN + 1];
    bool skip_at_check;
    INSERT_PADDING_BYTES(171);
};

/**
 * The size of this structure (0x400) has been verified via reverse engineering of multiple games
 * that use the software keyboard.
 */
static_assert(sizeof(SoftwareKeyboardConfig) == 0x400, "Software Keyboard Config size is wrong");

class SoftwareKeyboard final : public Applet {
public:
    SoftwareKeyboard(Service::APT::AppletId id) : Applet(id), started(false) { }

    ResultCode ReceiveParameter(const Service::APT::MessageParameter& parameter) override;
    ResultCode StartImpl(const Service::APT::AppletStartupParameter& parameter) override;
    void Update() override;
    bool IsRunning() const override { return started; }

    /**
     * Draws a keyboard to the current bottom screen framebuffer.
     */
    void DrawScreenKeyboard();

    /**
     * Sends the LibAppletClosing signal to the application,
     * along with the relevant data buffers.
     */
    void Finalize();

    /// This SharedMemory will be created when we receive the LibAppJustStarted message.
    /// It holds the framebuffer info retrieved by the application with GSPGPU::ImportDisplayCaptureInfo
    Kernel::SharedPtr<Kernel::SharedMemory> framebuffer_memory;

    /// SharedMemory where the output text will be stored
    Kernel::SharedPtr<Kernel::SharedMemory> text_memory;

    /// Configuration of this instance of the SoftwareKeyboard, as received from the application
    SoftwareKeyboardConfig config{};

    /// Whether this applet is currently running instead of the host application or not.
    bool started;
};

}
} // namespace
