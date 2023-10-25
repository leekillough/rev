#ifndef _ZOP_EVENT_H_
#define _ZOP_EVENT_H_

#include <sst/core/event.h>
#include <sst/core/interfaces/stdMem.h>

using namespace SST::Interfaces;

// --------------------------------------
// basicMsgEvent Event handler
// --------------------------------------

namespace SST {
namespace ForzaElement {

class ZOPMsg
{
    public:
    uint32_t Header;
    uint32_t DestID;
    uint32_t SrcID;
    uint32_t AppID;
    uint32_t msg_id;
    std::vector<uint64_t> payload;
    Interfaces::StandardMem::Request *rqst;
};

class basicZOPEvent : public Event, public ZOPMsg
{
public:
    // int seq;
    basicZOPEvent() : SST::Event()
    {
    }

    // Overloaded Constructor
    basicZOPEvent(ZOPMsg zop) : SST::Event(), ZOP(zop)
    {
    }

    // Event* clone(void) override
    // {
    //     return new basicZOPEvent(*this);
    // }

    void serialize_order(SST::Core::Serialization::serializer &ser)  override
    {
        Event::serialize_order(ser);
        ser & Header;
        ser & DestID;
        ser & SrcID;
        ser & AppID;
        //ser & rqst;
    }

    // virtual void print(const std::string& header, Output &out) const  override {
    //     out.output("%s basicZOPEvent to be delivered at %" PRIu64 " with priority %d.\n",
    //                header.c_str(), getDeliveryTime(), getPriority());
    // }

    ZOPMsg getPayload()
    {
        return ZOP;
    }

private:
    ImplementSerializable(SST::ForzaElement::basicZOPEvent);

public:
    ZOPMsg ZOP;
};
}
}
#endif