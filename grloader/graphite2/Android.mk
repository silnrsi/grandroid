# LOCAL_PATH := $(APP_PROJECT_PATH)/jni/graphite

BASE_PATH := $(APP_PROJECT_PATH)/graphite2
include $(CLEAR_VARS)

GRHOME := ${BASE_PATH}/graphite_source
_NS := GR2
GR2_BASE := ${GRHOME}
GR2_MACHINE := direct
include ${GRHOME}/src/files.mk

SRC := ${BASE_PATH}/inc

LOCAL_MODULE := libgraphite2
LOCAL_SRC_FILES := $(GR2_SOURCES)
LOCAL_C_INCLUDES := ${GRHOME}/include ${GRHOME}/src \
        ${SRC}/stlport/stlport \
        ${SRC}/bionic
LOCAL_EXPORT_C_INCLUDES := ${GRHOME}/include
LOCAL_CFLAGS := -mapcs -DGRAPHITE2_NSEGCACHE -DGRAPHITE2_NTRACING -DNDEBUG
LOCAL_LDFLAGS := -L$(BASE_PATH)/../srclibs
include $(BUILD_SHARED_LIBRARY)

ifeq ($(TARGET_ARCH_ABI),armeabi)
include $(CLEAR_VARS)

LOCAL_MODULE := libgrload2
LOCAL_SRC_FILES := ${BASE_PATH}/grload2.cpp ${BASE_PATH}/grandroid.cpp ${BASE_PATH}/load.cpp
LOCAL_C_INCLUDES := ${GRHOME}/include \
        ${SRC}/stlport/stlport \
        ${SRC}/bionic \
        ${SRC}/skia/include/core \
        ${SRC}/frameworks/base/include \
        ${SRC}/system/core/include \
        ${SRC} \
        ${SRC}/skia/include/core \
        ${SRC}/skia/src/core \
        ${SRC}/harfbuzz/contrib \
        ${SRC}/harfbuzz/src \
        ${SRC}/freetype/include
#LOCAL_ARM_MODE := arm
LOCAL_CFLAGS := -mapcs -DANDROID_ARM_LINKER -DGRLOAD_API=10
LOCAL_CFLAGS += -include "$(SRC)/system/core/include/arch/linux-arm/AndroidConfig.h"
LOCAL_SHARED_LIBRARIES := libgraphite2
LOCAL_LDFLAGS := -L$(BASE_PATH)/srclibs/${TARGET_ARCH_ABI}
# the following are needed to support the jni function in grandroid.cpp
LOCAL_LDFLAGS += -lskia -lcutils -landroid -landroid_runtime -lutils -lft2_local
include $(BUILD_SHARED_LIBRARY)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
include $(CLEAR_VARS)

LOCAL_MODULE := libgrload4
LOCAL_SRC_FILES := ${BASE_PATH}/grload4.cpp ${BASE_PATH}/grandroid.cpp ${BASE_PATH}/load.cpp
LOCAL_C_INCLUDES := ${GRHOME}/include \
        ${SRC}/stlport/stlport \
        ${SRC}/bionic \
        ${SRC}/skia/include/core \
        ${SRC}/frameworks/base/include \
        ${SRC}/system/core/include \
        ${SRC} \
        ${SRC}/skia/include/core \
        ${SRC}/harfbuzz/contrib \
        ${SRC}/harfbuzz/src \
        ${SRC}/freetype/include
LOCAL_CFLAGS := -mapcs -DANDROID_ARM_LINKER -DGRLOAD_API=15
LOCAL_CFLAGS += -include "$(SRC)/system/core/include/arch/linux-arm/AndroidConfig.h"
LOCAL_SHARED_LIBRARIES := libgraphite2
LOCAL_LDFLAGS := -L$(BASE_PATH)/srclibs/${TARGET_ARCH_ABI}
# the following are needed to support the jni function in grandroid.cpp
LOCAL_LDFLAGS += -lharfbuzz -lskia -lcutils -landroid -landroid_runtime -lutils -lft2_local
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := libgrload43
LOCAL_SRC_FILES := ${BASE_PATH}/grload5.cpp ${BASE_PATH}/grload4.cpp ${BASE_PATH}/grandroid.cpp ${BASE_PATH}/load.cpp
LOCAL_C_INCLUDES := ${GRHOME}/include \
        ${SRC}/stlport/stlport \
        ${SRC}/bionic \
        ${SRC}/skia/include/core \
        ${SRC}/frameworks/base/include \
        ${SRC}/system/core/include \
        ${SRC} \
        ${SRC}/skia/include/core \
        ${SRC}/harfbuzz_ng/src \
        ${SRC}/harfbuzz/contrib \
        ${SRC}/harfbuzz/src \
        ${SRC}/freetype/include
LOCAL_CFLAGS := -mapcs -DANDROID_ARM_LINKER -DGRLOAD_API=18
LOCAL_CFLAGS += -include "$(SRC)/system/core/include/arch/linux-arm/AndroidConfig.h"
LOCAL_SHARED_LIBRARIES := libgraphite2
LOCAL_LDFLAGS := -L$(BASE_PATH)/srclibs/${TARGET_ARCH_ABI}
# the following are needed to support the jni function in grandroid.cpp
LOCAL_LDFLAGS += -lharfbuzz_ng -lharfbuzz -lskia -lcutils -landroid -landroid_runtime -lutils -lft2_local
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := libgrload5
LOCAL_SRC_FILES := ${BASE_PATH}/grload5.cpp ${BASE_PATH}/grandroid.cpp ${BASE_PATH}/load.cpp
LOCAL_C_INCLUDES := ${GRHOME}/include \
        ${SRC}/stlport/stlport \
        ${SRC}/bionic \
        ${SRC}/skia/include/core \
        ${SRC}/frameworks/base/include \
        ${SRC}/system/core/include \
        ${SRC} \
        ${SRC}/skia/include/core \
        ${SRC}/harfbuzz_ng/src \
        ${SRC}/freetype/include
LOCAL_CFLAGS := -mapcs -DANDROID_ARM_LINKER -DGRLOAD_API=19
LOCAL_CFLAGS += -include "$(SRC)/system/core/include/arch/linux-arm/AndroidConfig.h"
LOCAL_SHARED_LIBRARIES := libgraphite2
LOCAL_LDFLAGS := -L$(BASE_PATH)/srclibs/${TARGET_ARCH_ABI}
# the following are needed to support the jni function in grandroid.cpp
LOCAL_LDFLAGS += -lharfbuzz_ng -lskia -lcutils -landroid -landroid_runtime -lutils -lft2
include $(BUILD_SHARED_LIBRARY)
endif

