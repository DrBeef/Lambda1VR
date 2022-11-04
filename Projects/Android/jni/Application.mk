# MAKEFILE_LIST specifies the current used Makefiles, of which this is the last
# one. I use that to obtain the Application.mk dir then import the root
# Application.mk.
ROOT_DIR := $(dir $(lastword $(MAKEFILE_LIST)))../../../../..
NDK_MODULE_PATH := $(ROOT_DIR)

XASH_SDL ?= 0
APP_PLATFORM := android-19

APP_CFLAGS += -Wl,--no-undefined

# if non-zero, works only if single ABI selected
XASH_THREAD_NUM ?= 0


# CFLAGS_OPT :=  -O3 -fomit-frame-pointer -ggdb -funsafe-math-optimizations -ftree-vectorize -fgraphite-identity -floop-interchange -funsafe-loop-optimizations -finline-limit=256 -pipe
# CFLAGS_OPT_ARM := -mthumb -mfpu=neon -mcpu=cortex-a9 -pipe -mvectorize-with-neon-quad -DVECTORIZE_SINCOS -fPIC -DHAVE_EFFICIENT_UNALIGNED_ACCESS
# CFLAGS_HARDFP := -D_NDK_MATH_NO_SOFTFP=1 -mhard-float -mfloat-abi=hard -DLOAD_HARDFP -DSOFTFP_LINK
APPLICATIONMK_PATH = $(call my-dir)

GL4ES_PATH := $(APPLICATIONMK_PATH)/src/gl4es/include

XASH3D_PATH := $(APPLICATIONMK_PATH)/src/Xash3D/xash3d

HLSDK_PATH  := $(APPLICATIONMK_PATH)/src/HLSDK/halflife/

# XASH3D_CONFIG := $(APPLICATIONMK_PATH)/xash3d_config.mk

# ndk-r14 introduced failure for missing dependencies. If 'false', the clean
# step will error as we currently remove prebuilt artifacts on clean.
APP_ALLOW_MISSING_DEPS=true

APP_MODULES := gl4es xash menu client server client_bshift server_bshift client_opfor server_opfor client_aomdc server_aomdc client_theyhunger server_theyhunger
APP_STL := c++_shared


