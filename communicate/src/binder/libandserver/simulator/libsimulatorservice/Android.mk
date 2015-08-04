LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        ISimulatorService.cpp \
        SimulatorService.cpp

LOCAL_SHARED_LIBRARIES:= \
        libutils \
        libbinder \
        libhardware

LOCAL_C_INCLUDES:= \
         frameworks/simulator/include

LOCAL_MODULE:= libsimulatorservice

LOCAL_MODULE_TAGS:= optional

include $(BUILD_SHARED_LIBRARY)
