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

package com.android.server;

import android.content.Context;
import android.os.ISimulatorService;
import android.util.Slog;

public class SimulatorService extends ISimulatorService.Stub {
    private static final String TAG = "SimulatorService";
    private int mPtr = 0;

    SimulatorService() {
        mPtr = initNative();
        if (mPtr == 0) {
            Slog.e(TAG, "Failed to initialize simulator service.");
        }
    }

    public void setVal(int val) {
        if (mPtr == 0) {
            Slog.e(TAG, "Simulator service is not initialized.");
            return;
        }
        setValNative(mPtr, val);
    }

    public int getVal() {
        if (mPtr == 0) {
            Slog.e(TAG, "Simulator service is not initialized.");
            return -1;
        }

        return getValNative(mPtr);
    }

    private native int initNative();
    private native void setValNative(int ptr, int val);
    private native int getValNative(int ptr);
}