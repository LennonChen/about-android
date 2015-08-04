#ifndef ANDROID_ISIMULATOR_SERVICE_H
#define ANDROID_ISIMULATOR_SERVICE_H

#include <utils/RefBase.h>
#include <utils/String16.h>
#include <utils/Errors.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>

namespace android {

class ISimulatorService: public IInterface
{
public:
    DECLARE_META_INTERFACE(SimulatorService);

    virtual int32_t getVal( void) const = 0;
    virtual void    setVal( int32_t val) = 0;
};

class BnSimulatorService : public BnInterface<ISimulatorService>
{
public:
    virtual status_t onTransact( uint32_t code,
                                 const Parcel& data,
                                 Parcel* reply,
                                 uint32_t flags = 0);
};

};

#endif  // ANDROID_ISIMULATOR_SERVICE_H
