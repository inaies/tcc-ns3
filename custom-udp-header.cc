#include "custom-udp-header.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("CustomUdpHeader");

TypeId
CustomUdpHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::CustomUdpHeader")
        .SetParent<Header>()
        .AddConstructor<CustomUdpHeader>()
        .AddAttribute("CustomId", "Personalized identifier.",
                      UintegerValue(0),
                      MakeUintegerAccessor(&CustomUdpHeader::m_customId),
                      MakeUintegerChecker<uint32_t>())
    ;
    return tid;
}

TypeId
CustomUdpHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
CustomUdpHeader::SetCustomId(uint32_t id)
{
    m_customId = id;
}

uint32_t
CustomUdpHeader::GetCustomId() const
{
    return m_customId;
}

uint32_t
CustomUdpHeader::GetSerializedSize() const
{
    // Apenas o tamanho do uint32_t (4 bytes)
    return 4;
}

void
CustomUdpHeader::Serialize(Buffer::Iterator start) const
{
    // Escreve m_customId no buffer (em network order)
    start.WriteHtonU32(m_customId);
}

uint32_t
CustomUdpHeader::Deserialize(Buffer::Iterator start)
{
    // Lê m_customId do buffer (de network order)
    m_customId = start.ReadNtohU32();
    return GetSerializedSize();
}

void
CustomUdpHeader::Print(std::ostream& os) const
{
    os << "CustomUdpHeader(id=" << m_customId << ")";
}

} // namespace ns3
