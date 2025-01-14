//
// _zip_nic_h_
//

#ifndef _ZIP_NIC_H_
#define _ZIP_NIC_H_

#include "SST.h"
#include "zip_events.h"
#include <string>

namespace SST::Forza{
  // API for the HFI
  class nicAPI: public SST::SubComponent{
  public:
    SST_ELI_REGISTER_SUBCOMPONENT_API(SST::Forza::nicAPI)
    nicAPI(ComponentId_t id, Params& params) : SubComponent(id) { }
    virtual ~nicAPI() = default;
    virtual void setMsgHandler(Event::HandlerBase* handler) = 0;
    void init( unsigned int phase ) override          = 0;

    void setup() override = 0; 
    virtual void send(ZIPEvent *ev, int dest) = 0;
    virtual int getNumDestinations() = 0;
    virtual SST::Interfaces::SimpleNetwork::nid_t getAddress() = 0;
  };

  // HFI NIC for the ZIP (based on Rev NIC)
  class ZIPHFINIC : public nicAPI {
  public:
    // Register with the SST Core
    SST_ELI_REGISTER_SUBCOMPONENT(
      ZIPHFINIC,
      "ForzaZIP",
      "ZIPHFINIC",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "ZIP HFI NIC",
      SST::Forza::nicAPI
      )

    // Register the parameters
    SST_ELI_DOCUMENT_PARAMS(
      {"clock", "Clock frequency of the NIC", "1Ghz"},
      {"port", "Port to use, if loaded as an anonymous subcomponent", "network"},
      {"verbose", "Verbosity for output (0 = nothing)", "0"},
      )

    // Register the ports
    SST_ELI_DOCUMENT_PORTS(
      {"network", "Port to network", {"SST::Forza::ZIPEvent"} }
      )

    // Register the subcomponent slots
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
      {"iface", "SimpleNetwork interface to a network", "SST::Interfaces::SimpleNetwork"}
      )

    /// ZIPHFINIC: constructor
    ZIPHFINIC(ComponentId_t id, Params& params);

    /// ZIPHFINIC: destructor
    virtual ~ZIPHFINIC();

    /// ZIPHFINIC: Callback to parent on received messages
    void setMsgHandler( Event::HandlerBase* handler ) override;

    /// ZIPHFINIC: initialization function
    void init( unsigned int phase ) override;

    /// ZIPHFINIC: setup function
    void setup() override;

    /// ZIPHFINIC: send event to the destination id
    void send( ZIPEvent* ev, int dest ) override;

    /// ZIPHFINIC: retrieve the number of destinations
    int getNumDestinations() override;

    /// ZIPHFINIC: get the endpoint's network address
    SST::Interfaces::SimpleNetwork::nid_t getAddress() override;

    /// ZIPHFINIC: callback function for the SimpleNetwork interface
    bool msgNotify(int virtualNetwork);

    /// ZIPHFINIC: clock function
    virtual bool clockTick(Cycle_t cycle);

  protected:
    SST::Output* output;                    ///< ZIPHFINIC: SST output object

    SST::Interfaces::SimpleNetwork * iFace; ///< ZIPHFINIC: SST network interface

    SST::Event::HandlerBase *msgHandler;    ///< ZIPHFINIC: SST message handler

    bool initBroadcastSent;                 ///< ZIPHFINIC: has the init bcast been sent?

    int numDest;                            ///< ZIPHFINIC: number of SST destinations

    std::queue<SST::Interfaces::SimpleNetwork::Request*> sendQ; ///< ZIPHFINIC: buffered send queue
  };
} // namespace SST::Forza

#endif // _ZIP_NIC_H_

// EOF
