APP_PLATFORM := android-24

APPLICATIONMK_PATH = $(call my-dir)
NDK_MODULE_PATH := $(APPLICATIONMK_PATH)/../..

XASH_SDL ?= 0

APP_CFLAGS += -Wl,--no-undefined

# if non-zero, works only if single ABI selected
XASH_THREAD_NUM ?= 0

TOP_DIR			:= $(APPLICATIONMK_PATH)/src
GL4ES_PATH := $(APPLICATIONMK_PATH)/src/gl4es/include
XASH3D_PATH := $(APPLICATIONMK_PATH)/src/Xash3D/xash3d
HLSDK_PATH  := $(APPLICATIONMK_PATH)/src/HLSDK/halflife/

# ndk-r14 introduced failure for missing dependencies. If 'false', the clean
# step will error as we currently remove prebuilt artifacts on clean.
APP_ALLOW_MISSING_DEPS=true

APP_MODULES := gl4es xash menu client server client_bshift server_bshift client_opfor server_opfor client_aomdc server_aomdc client_theyhunger server_theyhunger
APP_STL := c++_shared


