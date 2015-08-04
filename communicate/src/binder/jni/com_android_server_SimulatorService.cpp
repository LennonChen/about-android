/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "SimulatorServiceJNI"
#include <utils/Log.h>

#include "jni.h"
#include "JNIHelp.h"
#include "android_runtime/AndroidRuntime.h"

#include <stdint.h>
#include <sys/types.h>

#include <hardware/hardware.h>
#include <hardware/simulator.h>

namespace android {

static jint android_server_SimulatorService_initNative(
    JNIEnv* env, jobject obj) {
    struct simulator_module_t* module = NULL;
    struct simulator_device_t* device = NULL;

    if (hw_get_module(SIMULATOR_MODULE_ID,
                      (const struct hw_module_t**)(&module))) {
        ALOGE("Failed to get module by id is '%s'", SIMULATOR_MODULE_ID);
        return 0;
    }
    if (((module->common).methods)->open(
            static_cast<const struct hw_module_t*>(&(module->common)),
            SIMULATOR_DEVICE_ID,
            (struct hw_device_t**)&device)) {
        ALOGE("Failed to get device by id is '%s'", SIMULATOR_DEVICE_ID);
        return 0;
    }

    return (jint)device;
}

static void android_server_SimulatorService_setValNative(
    JNIEnv* env, jobject obj, jint ptr, jint val) {
    struct simulator_device_t* device = (struct simulator_device_t*)ptr;

    if (device == NULL) {
        ALOGE("Simulator is not open.");
        return;
    }
    device->set_val(device, val);
}

static jint android_server_SimulatorService_getValNative(
    JNIEnv* env, jobject obj, jint ptr) {
    struct simulator_device_t* device = (struct simulator_device_t*)ptr;

    if (device == NULL) {
        ALOGE("Simulator is not open.");
        return -1;
    }

    jint val = -1;
    device->get_val(device, &val);

    return val;
}

static const JNINativeMethod sMethodTable[] = {
    {"initNative", "()I", (void*)android_server_SimulatorService_initNative},
    {"setValNative", "(II)V", (void*)android_server_SimulatorService_setValNative},
    {"getValNative", "(I)I", (void*)android_server_SimulatorService_getValNative},
};

int register_android_server_SimulatorService(JNIEnv* env) {
    return jniRegisterNativeMethods(env,
                                    "com/android/server/SimulatorService",
                                    sMethodTable,
                                    NELEM(sMethodTable));

}

} /* namespace android */
