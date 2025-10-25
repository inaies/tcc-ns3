#include "custom-packet-sink.h"
#include "ns3/log.h"
#include "ns3/udp-header.h" // Needed to process UDP payload
#include "ns3/ipv6-header.h" // Needed to process IPv6

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("CustomPacketSink");

TypeId
CustomPacketSink::GetTypeId()
{
    static TypeId tid = TypeId("ns3::CustomPacketSink")
        .SetParent<PacketSink>()
        .SetGroupName("Applications")
        .AddConstructor<CustomPacketSink>()
    ;
    return tid;
}

CustomPacketSink::CustomPacketSink()
{
}

CustomPacketSink::~CustomPacketSink()
{
}

void
CustomPacketSink::HandleRead(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        // 1. PacketSink Logic (Counts and records address)
        m_totalRx += packet->GetSize();
        m_lastAddress = from;
        
        // Convert the generic Address to the specific socket address type
        Inet6SocketAddress ipv6From = Inet6SocketAddress::ConvertFrom(from);
        
        // Log the reception (optional but helpful)
        NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s, node " 
                    << GetNode()->GetId() << " received " << packet->GetSize() 
                    << " bytes from " << ipv6From.GetIpv6() << ".");

        // 2. **Header Extraction Logic**
        
        // Remove lower layer headers first (IPv6 and UDP) to ensure your custom 
        // header is at the front of the remaining payload buffer.
        
        Ipv6Header ipv6Hdr;
        if (packet->RemoveHeader(ipv6Hdr))
        {
            // Successfully removed Ipv6 Header
        }

        UdpHeader udpHeader;
        if (packet->RemoveHeader(udpHeader))
        {
            // Successfully removed UDP Header
        }
        
        // Now, the remaining 'packet' buffer should start with your custom header.
        CustomUdpHeader customHeader;
        if (packet->RemoveHeader(customHeader))
        {
            // 3. **Process Custom Header Data**
            uint32_t senderId = customHeader.GetCustomId();
            NS_LOG_INFO("   --> Custom Header Found! Sender Node ID: " << senderId);
            
            // Here you can store or process the 'senderId' data.
        }
        else
        {
            NS_LOG_WARN("   --> WARNING: Custom Header NOT found in packet.");
        }
    }
}

} // namespace ns3
