// esp_lcd RGB-panel shim (simulator). Just enough types/functions for
// pt_display.h's inline VSYNC-restart code to compile natively.
#pragma once
#ifndef SIM_ESP_LCD_PANEL_RGB_H
#define SIM_ESP_LCD_PANEL_RGB_H

typedef void *esp_lcd_panel_handle_t;
typedef struct { int _unused; } esp_lcd_rgb_panel_event_data_t;
typedef bool (*esp_lcd_rgb_panel_vsync_cb_t)(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t *, void *);

typedef struct {
    esp_lcd_rgb_panel_vsync_cb_t on_vsync;
    void *on_bounce_empty;
    void *on_bounce_frame_finish;
} esp_lcd_rgb_panel_event_callbacks_t;

inline int esp_lcd_rgb_panel_register_event_callbacks(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_callbacks_t *, void *) { return 0; }
inline int esp_lcd_rgb_panel_restart(esp_lcd_panel_handle_t) { return 0; }

#endif
