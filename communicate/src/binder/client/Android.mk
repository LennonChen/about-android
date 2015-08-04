LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := SimulatorClient.cpp

LOCAL_SHARED_LIBRARIES:= \
        libsimulatorservice \
        libbinder \
        libutils \
        libcutils

LOCAL_C_INCLUDES:= \
        frameworks/simulator/include

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := simulatorclient

include $(BUILD_EXECUTABLE)
