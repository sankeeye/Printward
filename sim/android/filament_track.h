#pragma once
#ifndef FILAMENT_TRACK_H
#define FILAMENT_TRACK_H

// Scale-fed filament tracking (Android/tablet). A weighed spool sets a slot's
// capacity (filament grams); while a print runs, the active slot's used grams
// are estimated from the gcode's filament total x progress, so the remaining
// weight ticks down live. Values persist to /sdcard/pandatouch_weights.conf.
//
// Slot encoding matches Bambu's tray_now: 254 = external spool, otherwise
// unit*AMS_MAX_TRAYS + tray. capacity/used live in the existing storage globals
// (g_tray_capacity_g/g_tray_used_g/g_ext_*), which already drive the Filament
// screen, so the display updates for free.

extern int g_low_threshold_g;    // low-filament warning threshold (grams)

void  filament_track_init();     // load persisted weights + threshold
void  filament_track_loop();     // update the active slot's consumption estimate
void  filament_save();           // persist weights + threshold
void  filament_weigh_assign(int slot, float measured_g, float empty_g);  // set a roll
void  filament_clear_slot(int slot);   // no roll in this slot: capacity/used/price = 0

void  filament_set_price(int slot, float price_kg);  // remember a slot's EUR/kg
float filament_price(int slot);       // EUR/kg for the slot, 0 if unknown
float filament_remaining(int slot);   // grams left, or -1 if no capacity set
bool  filament_slot_low(int slot);    // capacity set AND remaining < threshold
bool  filament_any_low();             // any slot low

// While a print runs on a weighed slot: how many grams the active slot will
// fall SHORT for the rest of this print. >0 = shortage, 0 = enough, -1 = N/A
// (not printing / no gcode filament total / active slot not weighed).
float filament_shortfall();

#endif
