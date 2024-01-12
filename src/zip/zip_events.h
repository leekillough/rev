//
// _zip_events_h_
//

#ifndef _ZIP_EVENTS_H_
#define _ZIP_EVENTS_H_

#include "SST.h"
#include "zip_events.h"
#include <string>

#include "ZOPNET.h"

namespace SST::Forza{
  // generic event class for the HFI
  // will either send aggregated ZOPs or credits
  class ZIPEvent : public SST::Event {
    public:
      ZIPEvent() : SST::Event() {}

      uint64_t dest;
      uint64_t src;

      virtual Event* clone(void) override;

    private:
      ImplementSerializable(SST::Forza::ZIPEvent);
  };

  // event class for sending a collection of ZOPs over the HFI
  // This event stores ZOPs as their concatenated packets in a 64-bit vector.
  class ZIPAggEvent : public ZIPEvent {
    public:
      ZIPAggEvent() : ZIPEvent() {}
      ZIPAggEvent(std::vector<uint64_t> payload) : ZIPEvent(), payload(payload) {}

      std::vector<uint64_t> getPayload();

      // return a vector of zopEvents by breaking up payload according to the lengths stored in the header flits
      std::vector<zopEvent> getZOPs();

      void serialize_order(SST::Core::Serialization::serializer& _serializer) override;

    private:
      std::vector<uint64_t> payload;

      ImplementSerializable(SST::Forza::ZIPAggEvent);
  };

  // event class for sending credit updates over HFI
  class ZIPCreditEvent : public ZIPEvent {
    public:
      ZIPCreditEvent() : ZIPEvent(), credits() {}
      ZIPCreditEvent(int credits) : ZIPEvent(), credits(credits) {}

      int getCredits();

      void serialize_order(SST::Core::Serialization::serializer& _serializer) override;

    private:
      int credits;

      ImplementSerializable(SST::Forza::ZIPCreditEvent);
  };
}

#endif // _ZIP_EVENTS_H_

// EOF
