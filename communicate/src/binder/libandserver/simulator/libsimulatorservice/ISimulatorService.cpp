#include "ISimulatorService.h"

#define LOG_TAG "SimulatorService"
#include <utils/Log.h>

#include <stdint.h>
#include <sys/types.h>

namespace android {

enum {
    GET_VAL = IBinder::FIRST_CALL_TRANSACTION,
    SET_VAL
};

class BpSimulatorService : public BpInterface<ISimulatorService>
{
public:
    BpSimulatorService(const sp<IBinder>& impl)
        : BpInterface<ISimulatorService>(impl)
    {
    }

    virtual int32_t getVal(void) const
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISimulatorService::getInterfaceDescriptor());
        remote()->transact(GET_VAL, data, &reply);
        return reply.readInt32();
    }

    virtual void setVal(int32_t val)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISimulatorService::getInterfaceDescriptor());
        data.writeInt32(val);
        remote()->transact(SET_VAL, data, &reply);
    }
};

IMPLEMENT_META_INTERFACE(SimulatorService, "liminghao.xiaomi.ISimulatorService");

status_t BnSimulatorService::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags )
{
    switch(code) {
        case GET_VAL: {
            CHECK_INTERFACE(ISimulatorService, data, reply);
            int32_t val = getVal();
            reply->writeInt32(val);
            return NO_ERROR;
        } break;
        case SET_VAL: {
            CHECK_INTERFACE(ISimulatorService, data, reply);
            int val = data.readInt32();
            setVal(val);
            return NO_ERROR;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

};
