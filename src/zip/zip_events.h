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
  // will either send aggregated ZOPs or credits or rendezvous ack
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

  // event class for rendezvous RTS and CTS
  class ZIPRVEvent : public ZIPEvent {
    public:
      ZIPRVEvent() : ZIPEvent(), size(), CTS(false) {}
      ZIPRVEvent(uint64_t size) : ZIPEvent(), size(size), CTS(false) {}
      ZIPRVEvent(bool CTS) : ZIPEvent(), size(), CTS(CTS) {}

      uint64_t getSize();

      bool isCTS();

      void serialize_order(SST::Core::Serialization::serializer& _serializer) override;

    private:
      uint64_t size;

      bool CTS;

      ImplementSerializable(SST::Forza::ZIPRVEvent);
  };

  // event class for sending collection of ZOPs over the HFI with rendezvous
  class ZIPAggRVEvent : public ZIPEvent {
    public:
      ZIPAggRVEvent() : ZIPEvent(), num(), payload() {}
      ZIPAggRVEvent(uint64_t num, std::vector<uint64_t> payload) : ZIPEvent(), num(num), payload(payload) {}

      uint64_t getNum();

      std::vector<uint64_t> getPayload();

      void serialize_order(SST::Core::Serialization::serializer& _serializer) override;

    private:
      uint64_t num;

      std::vector<uint64_t> payload;

      ImplementSerializable(SST::Forza::ZIPAggRVEvent);
  };
}

#endif // _ZIP_EVENTS_H_

// EOF
