#include "ns3/packet-sink.h"
#include "ns3/ipv6-address.h"
#include "custom-udp-header.h" // Include your custom header

namespace ns3 {

class CustomPacketSink : public PacketSink
{
public:
    static TypeId GetTypeId();

    CustomPacketSink();
    virtual ~CustomPacketSink();

protected:
    // Override HandleRead to process the incoming packet
    virtual void HandleRead(Ptr<Socket> socket);
};

} // namespace ns3
