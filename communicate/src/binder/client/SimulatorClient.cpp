#define LOG_TAG "simulatorclient"

#include <stdint.h>
#include <stdio.h>

#include <utils/Log.h>
#include <binder/IServiceManager.h>

#include "ISimulatorService.h"

using namespace android;

int main(int argc, char** argv)
{
    sp<IBinder> binder = defaultServiceManager()->getService(
        String16("liminghao.xiaomi.SimulatorService"));
    if (binder == NULL) {
        ALOGE("Failed to get SimulatorService");
        return -1;
    }
    sp<ISimulatorService> service = ISimulatorService::asInterface(binder);
    if (service == NULL) {
        ALOGE("Failed to put binder convert to service");
        return -1;
    }
    printf("Read original value from SimulatorService:\n");
    int32_t val = service->getVal();
    printf("%d.\n", val);
    printf("Add value 1 to SimulatorService.\n");
    val++;
    service->setVal(val);
    printf("Read the value from SimulatorService again:\n");
    val = service->getVal();
    printf("%d.\n",val);

    return 0;
}
