#pragma once

#if DISPLAY_ENABLED


#include "pico/stdlib.h"
#include "hardware/i2c.h"

extern "C" {
    #include "ssd1306.h"
    #include "font.h"
}

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace Pico {

enum ScreenMode {
    SCREEN_MODE_CONSOLE,
    SCREEN_MODE_PD,
    SCREEN_MODE_STATUS
};

// All methods must be called from Core 0.
class Screen {
public:
    static void init(ssd1306_t* disp_ptr, i2c_inst_t* i2c, uint sda, uint scl, uint16_t width, uint16_t height, ScreenMode mode) {
        _disp = disp_ptr;
        _i2c = i2c;
        _width = width;
        _height = height;
        _mode = mode;

        i2c_init(_i2c, 400 * 1000);
        gpio_set_function(sda, GPIO_FUNC_I2C);
        gpio_set_function(scl, GPIO_FUNC_I2C);
        gpio_pull_up(sda);
        gpio_pull_up(scl);

        _disp->external_vcc = false;
        // Check both common I2C addresses
        if (!ssd1306_init(_disp, _width, _height, 0x3C, _i2c)) {
            ssd1306_init(_disp, _width, _height, 0x3D, _i2c);
        }

        ssd1306_clear(_disp);
        ssd1306_show(_disp);
        _dirty = true;
    }

    static void write_char(char c) {
        if (!_disp || _mode != SCREEN_MODE_STATUS) return;

        // Line wrap at 21 chars or manual newline
        if (c == '\n' || _char_idx >= 21) {
            _char_idx = 0;
            for (int i = 0; i < 3; i++) {
                memcpy(_log_lines[i], _log_lines[i + 1], 22);
            }
            memset(_log_lines[3], 0, 22);
            _dirty = true;
            if (c == '\n') return;
        }

        if (c >= 32 && c <= 126) {
            _log_lines[3][_char_idx++] = c;
            _log_lines[3][_char_idx] = '\0';
            _dirty = true;
        }
    }

        // Update a value in the PD grid. Does not change focus.
    static void update_value(int id, float val) {
        if (!_disp || _mode != SCREEN_MODE_PD) return;
        if (id < 0 || id >= 4) return;

        _pd_cache[id] = val;
        _dirty = true;
    }

    // Update the focused value in Console mode.
    static void update_focus(int id, float val) {
        if (!_disp || _mode != SCREEN_MODE_CONSOLE) return;

        _current_id = id;
        _current_val = val;
        _interaction_timer = to_ms_since_boot(get_absolute_time());
        _dirty = true;
    }

    // Output of a Pd [print] object (ignored in PD grid mode)
    static void show_print(const char* name, const char* text) {
        if (!_disp) return;

        if (_mode == SCREEN_MODE_STATUS) {
            // New line first, so the newest message stays at the bottom
            if (_log_lines[3][0] != '\0') write_char('\n');
            write_string(name);
            write_string(": ");
            write_string(text);
        } else if (_mode == SCREEN_MODE_CONSOLE) {
            snprintf(_print_buf, sizeof(_print_buf), "%s:%s", name, text);
            _current_id = ID_PRINT;
            _interaction_timer = to_ms_since_boot(get_absolute_time());
            _dirty = true;
        }
    }

    static void process(uint32_t now, const char** names, int num_names) {
        if (!_disp || (now - _last_draw_time < 40)) return;

        // Signed difference: the timer can be slightly newer than 'now'
        const bool showing =
            _current_id != ID_NONE &&
            (int32_t)(now - _interaction_timer) < 2000;

        // Console mode also redraws when the focus view times out
        if (_mode == SCREEN_MODE_CONSOLE && showing != _was_showing) _dirty = true;

        // ssd1306_show() blocks Core 0, so only draw on changes
        if (!_dirty) return;

        _dirty = false;
        _was_showing = showing;
        _last_draw_time = now;

        ssd1306_clear(_disp);

        if (_mode == SCREEN_MODE_STATUS) {
            for (int i = 0; i < 4; i++) {
                if (_log_lines[i][0] != '\0') {
                    ssd1306_draw_string(_disp, 0, i * 14, 1, _log_lines[i]);
                }
            }
        } else if (_mode == SCREEN_MODE_PD) {
            render_pd_grid(names, num_names);
        } else {
            render_console(showing, names, num_names);
        }

        ssd1306_show(_disp);
    }

private:
    static constexpr int ID_NONE = -1;
    static constexpr int ID_PRINT = -2;

    static void write_string(const char* s) {
        while (*s) write_char(*s++);
    }

    // Large font if the text fits the width, small font otherwise
    static void draw_focus(const char* text) {
        const int scale = (strlen(text) * 12 + 4 <= _width) ? 2 : 1;
        ssd1306_draw_string(_disp, 4, (_height / 2) - 4 * scale, scale, text);
    }

    static void render_console(bool showing, const char** names, int num_names) {
        if (!showing) {
            ssd1306_draw_string(_disp, 12, (_height / 2) - 12, 3, "PikoPD");
            return;
        }

        if (_current_id == ID_PRINT) {
            draw_focus(_print_buf);
            return;
        }

        snprintf(_display_buf, sizeof(_display_buf), "%s:%.2f",
            (_current_id >= 0 && _current_id < num_names) ? names[_current_id] : "val",
            _current_val);
        draw_focus(_display_buf);
    }

    static void render_pd_grid(const char** names, int num_names) {
    for (int i = 0; i < 4 && i < num_names; i++) {

        int x = (i % 2) * (_width / 2) + 4;
        int y = (i / 2) * (_height / 2) + 4;

        snprintf(_display_buf, sizeof(_display_buf), "%.1f", _pd_cache[i]);
        ssd1306_draw_string(_disp, x, y, 2, _display_buf);

        if (names[i][0] != '\0') {
            ssd1306_draw_string(_disp, x, y + 18, 1, names[i]);
        }
    }
}

    static ssd1306_t* _disp;
    static i2c_inst_t* _i2c;
    static ScreenMode _mode;
    static uint16_t _width, _height;
    static char _display_buf[32], _print_buf[40], _log_lines[4][22];
    static int _char_idx, _current_id;
    static float _current_val, _pd_cache[4];
    static uint32_t _interaction_timer, _last_draw_time;
    static bool _dirty, _was_showing;
};

// Static member definitions
inline ssd1306_t* Screen::_disp = nullptr;
inline i2c_inst_t* Screen::_i2c = nullptr;
inline ScreenMode Screen::_mode = SCREEN_MODE_CONSOLE;
inline uint16_t Screen::_width = 128;
inline uint16_t Screen::_height = 64;
inline char Screen::_display_buf[32] = {0};
inline char Screen::_print_buf[40] = {0};
inline char Screen::_log_lines[4][22] = {"", "", "", ""};
inline int Screen::_char_idx = 0;
inline float Screen::_current_val = 0.0f;
inline float Screen::_pd_cache[4] = {0.0f, 0.0f, 0.0f, 0.0f};
inline int Screen::_current_id = -1;
inline uint32_t Screen::_interaction_timer = 0;
inline uint32_t Screen::_last_draw_time = 0;
inline bool Screen::_dirty = true;
inline bool Screen::_was_showing = false;

} // namespace Pico

#endif
