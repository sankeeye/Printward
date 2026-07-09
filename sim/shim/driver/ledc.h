// ESP-IDF LEDC (PWM backlight) shim for the simulator. Enums/structs match the
// field order pt_display.h uses in its designated initializers; functions are
// no-op stubs (the sim has no real backlight).
#pragma once
#ifndef SIM_DRIVER_LEDC_H
#define SIM_DRIVER_LEDC_H

#include <cstdint>

typedef enum { LEDC_LOW_SPEED_MODE = 0, LEDC_HIGH_SPEED_MODE = 1 } ledc_mode_t;
typedef enum { LEDC_CHANNEL_0 = 0 } ledc_channel_t;
typedef enum { LEDC_TIMER_0 = 0, LEDC_TIMER_1 = 1 } ledc_timer_t;
typedef enum { LEDC_TIMER_11_BIT = 11 } ledc_timer_bit_t;
typedef enum { LEDC_USE_APB_CLK = 0, LEDC_AUTO_CLK = 1 } ledc_clk_cfg_t;
typedef enum { LEDC_INTR_DISABLE = 0 } ledc_intr_type_t;

typedef struct {
    ledc_mode_t speed_mode;
    ledc_timer_bit_t duty_resolution;
    ledc_timer_t timer_num;
    uint32_t freq_hz;
    ledc_clk_cfg_t clk_cfg;
} ledc_timer_config_t;

typedef struct {
    int gpio_num;
    ledc_mode_t speed_mode;
    ledc_channel_t channel;
    ledc_intr_type_t intr_type;
    ledc_timer_t timer_sel;
    uint32_t duty;
    int hpoint;
} ledc_channel_config_t;

inline int ledc_set_duty(ledc_mode_t, ledc_channel_t, uint32_t) { return 0; }
inline int ledc_update_duty(ledc_mode_t, ledc_channel_t) { return 0; }
inline int ledc_timer_config(const ledc_timer_config_t *) { return 0; }
inline int ledc_channel_config(const ledc_channel_config_t *) { return 0; }
inline int ledc_fade_func_install(int) { return 0; }

#endif
