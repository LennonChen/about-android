#define LOG_TAG "simulatorserver"
#include <utils/Log.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>

#include "SimulatorService.h"

using namespace android;

int main(int argc, char** argv)
{
    SimulatorService::instantiate();
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();

    return 0;
}
