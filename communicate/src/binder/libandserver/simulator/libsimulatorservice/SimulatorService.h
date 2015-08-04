#ifndef ANDROID_SIMULATORSERVICE_H
#define ANDROID_SIMULATORSERVICE_H

#include <ISimulatorService.h>
#include <hardware/simulator.h>

namespace android {

class SimulatorService : public BnSimulatorService
{
 public:
    static  void    instantiate(void);

    virtual int32_t getVal(void) const;
    virtual void    setVal(int32_t val);

 private:
    bool     initialize(void);

             SimulatorService(void);
    virtual ~SimulatorService(void);

    struct simulator_module_t* mModule;
    struct simulator_device_t* mDevice;
};

};

#endif  // ANDROID_SIMULATORSERVICE_H
