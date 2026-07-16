APP_STL := c++_shared
APP_ABI := armeabi-v7a arm64-v8a
APP_PLATFORM := android-19
APP_CPPFLAGS := -std=c++17 -fexceptions
# Use @response-files for ar/linker so the ~419 LVGL objects don't blow past
# the Windows command-line length limit.
APP_SHORT_COMMANDS := true