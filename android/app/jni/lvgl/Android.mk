LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := lvgl

# Recursive wildcard so we don't have to list all ~400 LVGL sources by hand.
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
ALL_LVGL_C := $(call rwildcard,$(LOCAL_PATH)/src,*.c)
LOCAL_SRC_FILES := $(ALL_LVGL_C:$(LOCAL_PATH)/%=%)

# lv_conf.h lives one level up (app/jni); the SDL headers are needed by LVGL's
# SDL driver (LV_SDL_INCLUDE_PATH = <SDL.h>).
LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/.. $(LOCAL_PATH)/../SDL/include

LOCAL_CFLAGS := -DLV_CONF_INCLUDE_SIMPLE -O2 -w

include $(BUILD_STATIC_LIBRARY)
