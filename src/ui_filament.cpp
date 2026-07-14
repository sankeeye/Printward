#include "ui_filament.h"
#include "ui_printer.h"
#include "bambu_mqtt.h"
#include "storage.h"
#include <lvgl.h>
#include "ui_scale.h"   // tablet font scaling (Android only)
#ifdef __ANDROID__
#include "spool_db.h"   // pick a saved roll for an AMS slot (tablet)
#include <cstdint>

// --- Roll picker ---------------------------------------------------------
// A screen-independent pop-up (built on lv_layer_top()) to assign a saved spool
// to an AMS/external slot. It used to live on a dedicated Filament tab; that tab
// was removed (the Dashboard already shows the AMS slots), so tapping a slot on
// the Dashboard opens this directly. Built once at startup via
// filament_roll_picker_init(); a dimmed backdrop catches taps outside to close.
#define EXT_SLOT 254

static lv_obj_t* g_spool_backdrop = nullptr;   // full-screen dim + tap-to-close
static lv_obj_t* g_spool_modal = nullptr;
static lv_obj_t* g_spool_list = nullptr;
static lv_obj_t* g_spool_title = nullptr;
static int g_pick_slot = -1;

static void picker_hide() {
    if (g_spool_backdrop) lv_obj_add_flag(g_spool_backdrop, LV_OBJ_FLAG_HIDDEN);
}
static void spool_choose_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i >= 0 && i < g_spool_count && g_pick_slot >= 0) spool_load_to_slot(i, g_pick_slot);
    picker_hide();
    update_printer_ui();
}
static void spool_clear_cb(lv_event_t*) {
    if (g_pick_slot >= 0) spool_clear_slot(g_pick_slot);   // no roll: clear tracking
    picker_hide();
    update_printer_ui();
}
static void spool_close_cb(lv_event_t*) { picker_hide(); }

static void open_spool_modal(int slot) {
    if (!g_spool_modal || !g_spool_backdrop) return;
    g_pick_slot = slot;
    if (slot == EXT_SLOT) lv_label_set_text(g_spool_title, "Kies rol voor de externe spoel");
    else lv_label_set_text_fmt(g_spool_title, "Kies rol voor AMS%d T%d",
                               slot / AMS_MAX_TRAYS + 1, slot % AMS_MAX_TRAYS + 1);

    lv_obj_clean(g_spool_list);
    // "Empty" option first: this slot has no roll -> clear its weight tracking.
    {
        lv_obj_t* b = lv_btn_create(g_spool_list);
        lv_obj_set_size(b, lv_pct(100), PT_SZ(48));
        lv_obj_set_style_bg_color(b, lv_color_hex(0x3a2f2f), LV_PART_MAIN);
        lv_obj_set_style_pad_hor(b, PT_SZ(10), LV_PART_MAIN);
        lv_obj_set_style_pad_gap(b, PT_SZ(10), LV_PART_MAIN);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* sw = lv_obj_create(b);
        lv_obj_set_size(sw, PT_SZ(30), PT_SZ(24));
        lv_obj_set_style_radius(sw, 4, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0x555b63), LV_PART_MAIN);
        lv_obj_set_style_border_width(sw, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(sw, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_clear_flag(sw, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, "Leeg - geen rol in dit slot");
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_add_event_cb(b, spool_clear_cb, LV_EVENT_CLICKED, NULL);
    }
    if (g_spool_count == 0) {
        lv_obj_t* l = lv_label_create(g_spool_list);
        lv_label_set_text(l, "Nog geen rollen - maak ze aan op de Spools-tab.");
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0x999999), 0);
    }
    for (int i = 0; i < g_spool_count; i++) {
        Spool& s = g_spools[i];
        lv_obj_t* b = lv_btn_create(g_spool_list);
        lv_obj_set_size(b, lv_pct(100), PT_SZ(48));
        lv_obj_set_style_bg_color(b, lv_color_hex(0x2a323b), LV_PART_MAIN);
        lv_obj_set_style_pad_hor(b, PT_SZ(10), LV_PART_MAIN);
        lv_obj_set_style_pad_gap(b, PT_SZ(10), LV_PART_MAIN);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* sw = lv_obj_create(b);
        lv_obj_set_size(sw, PT_SZ(30), PT_SZ(24));
        lv_obj_set_style_radius(sw, 4, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, lv_color_hex(s.color & 0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(sw, 0, LV_PART_MAIN);
        lv_obj_clear_flag(sw, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text_fmt(l, "%s   %s   %.0f g", s.name, s.material, spool_live_grams(s));
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_add_event_cb(b, spool_choose_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    lv_obj_clear_flag(g_spool_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_spool_backdrop);
}

void filament_open_roll_picker(int slot) { open_spool_modal(slot); }

void filament_roll_picker_init() {
    if (g_spool_backdrop) return;   // build once

    // Full-screen dimmed backdrop on the top layer; a tap outside the card closes.
    g_spool_backdrop = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_spool_backdrop);
    lv_obj_set_size(g_spool_backdrop, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(g_spool_backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_spool_backdrop, LV_OPA_50, 0);
    lv_obj_clear_flag(g_spool_backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_spool_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_spool_backdrop, spool_close_cb, LV_EVENT_CLICKED, NULL);

    g_spool_modal = lv_obj_create(g_spool_backdrop);
    lv_obj_set_size(g_spool_modal, PT_SZ(540), PT_SZ(400));
    lv_obj_center(g_spool_modal);
    lv_obj_set_style_bg_color(g_spool_modal, lv_color_hex(0x161b21), LV_PART_MAIN);
    lv_obj_set_style_border_color(g_spool_modal, lv_color_hex(0x3465a4), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_spool_modal, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(g_spool_modal, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_spool_modal, PT_SZ(14), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(g_spool_modal, PT_SZ(10), LV_PART_MAIN);
    lv_obj_set_flex_flow(g_spool_modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(g_spool_modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* sm_head = lv_obj_create(g_spool_modal);
    lv_obj_set_size(sm_head, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(sm_head, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(sm_head, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sm_head, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(sm_head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sm_head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(sm_head, LV_OBJ_FLAG_SCROLLABLE);
    g_spool_title = lv_label_create(sm_head);
    lv_label_set_text(g_spool_title, "Kies rol");
    lv_obj_set_style_text_font(g_spool_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(g_spool_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_t* sm_close = lv_btn_create(sm_head);
    lv_obj_set_size(sm_close, PT_SZ(90), PT_SZ(34));
    lv_obj_set_style_bg_color(sm_close, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_t* sm_cl = lv_label_create(sm_close); lv_label_set_text(sm_cl, "Sluit");
    lv_obj_set_style_text_font(sm_cl, &lv_font_montserrat_14, 0); lv_obj_center(sm_cl);
    lv_obj_add_event_cb(sm_close, spool_close_cb, LV_EVENT_CLICKED, NULL);

    g_spool_list = lv_obj_create(g_spool_modal);
    lv_obj_set_width(g_spool_list, lv_pct(100));
    lv_obj_set_flex_grow(g_spool_list, 1);
    lv_obj_set_style_bg_opa(g_spool_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_spool_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_spool_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(g_spool_list, PT_SZ(6), LV_PART_MAIN);
    lv_obj_set_flex_flow(g_spool_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(g_spool_list, LV_DIR_VER);
}
#endif
