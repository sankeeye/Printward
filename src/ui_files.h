#pragma once
#ifndef UI_FILES_H
#define UI_FILES_H

// Builds (or rebuilds) the Files screen - browse the printer's SD card /
// internal "cache" folder over FTPS, and start printing a .3mf or .gcode
// file already there - and switches to it.
void create_files_ui();

// Call every loop() iteration. No-op unless the Files screen is open and a
// folder listing or print-start was queued from a button tap; those are
// deferred here (rather than run directly from the click callback) since
// bambu_ftp_list() and bambu_cmd_start_*() are both slow/blocking enough to
// tear the frame if run from inside lv_task_handler().
void ui_files_loop();

#endif
