LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := mbedtls

# Only the core library. The bundled everest/p256-m 3rd-party crypto is left
# out: it's disabled in the config, and everest uses __int128 which armeabi-v7a
# (32-bit ARM) doesn't have. mbedTLS uses its generic ECP instead.
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
MBEDTLS_C := $(call rwildcard,$(LOCAL_PATH)/library,*.c)
LOCAL_SRC_FILES := $(subst $(LOCAL_PATH)/,,$(MBEDTLS_C))

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/library
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_CFLAGS := -w -O2

include $(BUILD_STATIC_LIBRARY)
