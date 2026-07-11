LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL
LVGL_PATH := ../lvgl

# The real UI + mocks live under ourui/ (synced from the repo's src/ and sim/
# by android_build.ps1). Shim headers come first so <Arduino.h>, <WiFi.h> etc.
# resolve to the stubs; ourui/src next for the real ui/mqtt/storage headers.
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/ourui/shim \
    $(LOCAL_PATH)/ourui/src \
    $(LOCAL_PATH)/pubsubclient \
    $(LOCAL_PATH)/arduinojson \
    $(LOCAL_PATH)/../mbedtls/include \
    $(LOCAL_PATH)/$(LVGL_PATH) \
    $(LOCAL_PATH)/$(SDL_PATH)/include \
    $(LOCAL_PATH)/..

# android_glue.cpp provides the storage globals + config loader + FTP stub;
# bambu_mqtt.cpp is the REAL printer client (over the mbedTLS WiFiClientSecure
# shim + PubSubClient). mocks.cpp is intentionally NOT compiled here.
LOCAL_SRC_FILES := \
    ourui/main.cpp \
    ourui/android_glue.cpp \
    android_compat.c \
    ourui/src/bambu_mqtt.cpp \
    ourui/src/bambu_ftp.cpp \
    pubsubclient/PubSubClient.cpp \
    ourui/src/ui_printer.cpp \
    ourui/src/ui_files.cpp \
    ourui/src/ui_settings.cpp \
    ourui/src/ui_filament.cpp \
    ourui/src/ui_move.cpp \
    ourui/src/ui_wifi.cpp \
    ourui/src/ui_tablet_setup.cpp \
    ourui/src/ui_screensaver.cpp \
    ourui/src/gcode_view.cpp

LOCAL_CFLAGS := -DLV_CONF_INCLUDE_SIMPLE -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -w

LOCAL_SHARED_LIBRARIES := SDL2
LOCAL_STATIC_LIBRARIES := lvgl mbedtls

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid -lz

include $(BUILD_SHARED_LIBRARY)
