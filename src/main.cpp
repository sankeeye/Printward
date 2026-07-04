#include "pt/pt_display.h"
#include "printer_app.h"
#include "storage.h"
#include "ui_printer.h"
#include "esp32-hal-cpu.h"

void setup()
{
  setCpuFrequencyMhz(240);

  pinMode(21, OUTPUT); digitalWrite(21, 0);
  delay(500);

  Serial.begin(115200);
  delay(200);
  Serial.println("\n\n=== PandaTouch Bambu P1S Panel Starting ===");

  // PARTIAL_2_PSRAM only redraws the small dirty region on each update
  // (e.g. just the button that was pressed) instead of the whole screen,
  // which avoids visible tearing on this continuously-scanned RGB panel.
  pt_setup_display(PT_LVGL_RENDER_PARTIAL_2_PSRAM);
  PrinterApp::setup();
  pt_set_backlight(g_brightness, true);
}

void set_brightness(uint8_t val) {
  pt_set_backlight(val, true);
}

void loop()
{
  pt_loop_display();
  PrinterApp::loop();
}
