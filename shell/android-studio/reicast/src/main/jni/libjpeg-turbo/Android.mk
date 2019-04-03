LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libjpeg-turbo
SOURCE_PATH := $(LOCAL_PATH)/src
LOCAL_SRC_FILES += \
	$(SOURCE_PATH)/jaricom.c \
	$(SOURCE_PATH)/jcapimin.c \
	$(SOURCE_PATH)/jcapistd.c \
	$(SOURCE_PATH)/jcarith.c \
	$(SOURCE_PATH)/jccoefct.c \
	$(SOURCE_PATH)/jccolor.c \
	$(SOURCE_PATH)/jcdctmgr.c \
	$(SOURCE_PATH)/jchuff.c \
	$(SOURCE_PATH)/jcinit.c \
	$(SOURCE_PATH)/jcmainct.c \
	$(SOURCE_PATH)/jcmarker.c \
	$(SOURCE_PATH)/jcmaster.c \
	$(SOURCE_PATH)/jcomapi.c \
	$(SOURCE_PATH)/jcparam.c \
	$(SOURCE_PATH)/jcphuff.c \
	$(SOURCE_PATH)/jcprepct.c \
	$(SOURCE_PATH)/jcsample.c \
	$(SOURCE_PATH)/jctrans.c \
	$(SOURCE_PATH)/jdapimin.c \
	$(SOURCE_PATH)/jdapistd.c \
	$(SOURCE_PATH)/jdarith.c \
	$(SOURCE_PATH)/jdatadst.c \
	$(SOURCE_PATH)/jdatasrc.c \
	$(SOURCE_PATH)/jdcoefct.c \
	$(SOURCE_PATH)/jdcolor.c \
	$(SOURCE_PATH)/jddctmgr.c \
	$(SOURCE_PATH)/jdhuff.c \
	$(SOURCE_PATH)/jdinput.c \
	$(SOURCE_PATH)/jdmainct.c \
	$(SOURCE_PATH)/jdmarker.c \
	$(SOURCE_PATH)/jdmaster.c \
	$(SOURCE_PATH)/jmemnobs.c \
	$(SOURCE_PATH)/jdmerge.c \
	$(SOURCE_PATH)/jdphuff.c \
	$(SOURCE_PATH)/jdpostct.c \
	$(SOURCE_PATH)/jdsample.c \
	$(SOURCE_PATH)/jdtrans.c \
	$(SOURCE_PATH)/jerror.c \
	$(SOURCE_PATH)/jfdctflt.c \
	$(SOURCE_PATH)/jfdctfst.c \
	$(SOURCE_PATH)/jfdctint.c \
	$(SOURCE_PATH)/jidctflt.c \
	$(SOURCE_PATH)/jidctfst.c \
	$(SOURCE_PATH)/jidctint.c \
	$(SOURCE_PATH)/jidctred.c \
	$(SOURCE_PATH)/jmemmgr.c \
	$(SOURCE_PATH)/jquant1.c \
	$(SOURCE_PATH)/jquant2.c \
	$(SOURCE_PATH)/jutils.c \
	$(SOURCE_PATH)/turbojpeg.c \
	$(SOURCE_PATH)/transupp.c \
	$(SOURCE_PATH)/jdatadst-tj.c \
	$(SOURCE_PATH)/jdatasrc-tj.c \
	$(SOURCE_PATH)/rdbmp.c \
	$(SOURCE_PATH)/rdppm.c \
	$(SOURCE_PATH)/wrbmp.c \
	$(SOURCE_PATH)/wrppm.c

LOCAL_CFLAGS += -I$(SOURCE_PATH) -I$(LOCAL_PATH)/include -DBMP_SUPPORTED -DPPM_SUPPORTED

ifeq ($(TARGET_ARCH_ABI),armeabi)
LOCAL_SRC_FILES += $(SOURCE_PATH)/jsimd_none.c
LOCAL_CFLAGS += -DSIZEOF_SIZE_T=4

else ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
# ARM v7 NEON
LOCAL_ARM_NEON := true
LOCAL_CFLAGS += -D__ARM_HAVE_NEON__
LOCAL_CFLAGS += -DSIZEOF_SIZE_T=4
LOCAL_SRC_FILES += \
	$(SOURCE_PATH)/simd/arm/jsimd.c \
	$(SOURCE_PATH)/simd/arm/jsimd_neon.S

else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
# ARM v8 64-bit NEON
LOCAL_ARM_NEON := true
LOCAL_CFLAGS += -D__ARM_HAVE_NEON__
LOCAL_CFLAGS += -DSIZEOF_SIZE_T=8
LOCAL_SRC_FILES += \
	$(SOURCE_PATH)/simd/arm64/jsimd.c \
    $(SOURCE_PATH)/simd/arm64/jsimd_neon.S

else ifeq ($(TARGET_ARCH_ABI),x86)
# x86 MMX and SSE2
LOCAL_CFLAGS += -DSIZEOF_SIZE_T=4
LOCAL_ASMFLAGS += -DPIC -DELF
# TODO
else ifeq ($(TARGET_ARCH_ABI),x86_64)
# x86-64 SSE2
LOCAL_CFLAGS += -DSIZEOF_SIZE_T=8
LOCAL_ASMFLAGS += -D__x86_64__ -DPIC -DELF
# TODO
endif

LOCAL_EXPORT_C_INCLUDES := $(SOURCE_PATH)
include $(BUILD_SHARED_LIBRARY)
