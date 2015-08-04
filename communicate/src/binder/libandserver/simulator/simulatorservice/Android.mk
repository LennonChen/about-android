LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        main_simulatorserver.cpp

LOCAL_SHARED_LIBRARIES:= \
        libsimulatorservice \
        libbinder \
        libutils

LOCAL_C_INCLUDES:= \
        frameworks/simulator/libsimulatorservice \
        frameworks/simulator/include

LOCAL_MODULE:= simulatorserver

LOCAL_MODULE_TAGS:= optional

include $(BUILD_EXECUTABLE)