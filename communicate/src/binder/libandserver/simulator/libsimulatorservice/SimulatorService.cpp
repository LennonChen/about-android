#define LOG_TAG "SimulatorService"
#include <utils/Log.h>

#include <stdint.h>
#include <sys/types.h>

#include <binder/IServiceManager.h>
#include "SimulatorService.h"

namespace android {

bool SimulatorService::initialize(void)
{
    mModule = NULL;
    mDevice = NULL;

    if (hw_get_module(
            SIMULATOR_MODULE_ID,
            (const struct hw_module_t**)(&mModule))) {
        ALOGE("Failed to get module by id is '%s'", SIMULATOR_MODULE_ID);
        return false;
    }
    if (((mModule->common).methods)->open(
            static_cast<const struct hw_module_t*>(&(mModule->common)),
            SIMULATOR_DEVICE_ID,
            (struct hw_device_t**)&mDevice)) {
        ALOGE("Failed to get device by id is '%s'", SIMULATOR_DEVICE_ID);
        return false;
    }
    return true;
}

SimulatorService::SimulatorService(void)
{
    initialize();
}

SimulatorService::~SimulatorService(void)
{
    if (mDevice != NULL ) {
        mDevice->common.close((struct hw_device_t*)mDevice);
    }
}

void SimulatorService::instantiate(void)
{
    defaultServiceManager()->addService(
        String16("liminghao.xiaomi.SimulatorService"),
        new SimulatorService());
}

int32_t SimulatorService::getVal(void) const
{
    if (mDevice == NULL) return NO_INIT;
    int32_t val = 0;
    mDevice->get_val(mDevice, &val);
    return val;
}

void SimulatorService::setVal(int32_t val)
{
    if (mDevice == NULL) return;
    mDevice->set_val(mDevice, val);
}

};
