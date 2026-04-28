#pragma once

#include "pico/stdlib.h"
#include "hardware/i2c.h"

extern "C" {
    #include "screen/ssd1306.h"
    #include "screen/font.h"
}

#include <stdio.h>
#include <string.h>

namespace Pico {

enum ScreenMode {
    SCREEN_MODE_CONSOLE,
    SCREEN_MODE_PD,
    SCREEN_MODE_STATUS
};

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
            if (c == '\n') return;
        }

        if (c >= 32 && c <= 126) {
            _log_lines[3][_char_idx++] = c;
            _log_lines[3][_char_idx] = '\0';
        }
    }

    static void update_focus(int id, float val) {
        _current_id = id;
        _current_val = val;
        _interaction_timer = to_ms_since_boot(get_absolute_time());
        if (id >= 0 && id < 4) _pd_cache[id] = val;
    }

    static void process(uint32_t now, const char** names, int num_names) {
        if (!_disp || (now - _last_draw_time < 40)) return;
        _last_draw_time = now;

        ssd1306_clear(_disp);

        if (_mode == SCREEN_MODE_STATUS) {
            for (int i = 0; i < 4; i++) { 
                if (_log_lines[i][0] != '\0') {
                    ssd1306_draw_string(_disp, 0, i * 14, 1, _log_lines[i]); 
                }
            }
        } else if (_mode == SCREEN_MODE_PD) {
            render_pd_grid(names);
        } else {
            render_console(now, names, num_names);
        }

        ssd1306_show(_disp);
    }

private:
    static void render_console(uint32_t now, const char** names, int num_names) {
        if (now - _interaction_timer < 2000 && _current_id != -1) {
            snprintf(_display_buf, 32, "%s:%.2f", (_current_id < num_names && _current_id >= 0) ? names[_current_id] : "val", _current_val);
            ssd1306_draw_string(_disp, 4, (_height / 2) - 8, 2, _display_buf);
        } else {
            ssd1306_draw_string(_disp, 12, (_height / 2) - 12, 3, "PikoPD");
        }
    }

    static void render_pd_grid(const char** names) {
        for (int i = 0; i < 4; i++) {
            int x = (i % 2) * (_width / 2) + 4;
            int y = (i / 2) * (_height / 2) + 4;
            snprintf(_display_buf, 32, "%.1f", _pd_cache[i]);
            ssd1306_draw_string(_disp, x, y, 2, _display_buf);
            if (names && i < 4) ssd1306_draw_string(_disp, x, y + 18, 1, names[i]);
        }
    }

    static ssd1306_t* _disp;
    static i2c_inst_t* _i2c;
    static ScreenMode _mode;
    static uint16_t _width, _height;
    static char _display_buf[32], _log_lines[4][22];
    static int _char_idx, _current_id;
    static float _current_val, _pd_cache[4];
    static uint32_t _interaction_timer, _last_draw_time;
};

// Static member definitions
inline ssd1306_t* Screen::_disp = nullptr;
inline i2c_inst_t* Screen::_i2c = nullptr;
inline ScreenMode Screen::_mode = SCREEN_MODE_CONSOLE;
inline uint16_t Screen::_width = 128;
inline uint16_t Screen::_height = 64;
inline char Screen::_display_buf[32] = {0};
inline char Screen::_log_lines[4][22] = {"", "", "", ""};
inline int Screen::_char_idx = 0;
inline float Screen::_current_val = 0.0f;
inline float Screen::_pd_cache[4] = {0.0f, 0.0f, 0.0f, 0.0f};
inline int Screen::_current_id = -1;
inline uint32_t Screen::_interaction_timer = 0;
inline uint32_t Screen::_last_draw_time = 0;

} // namespace Pico
