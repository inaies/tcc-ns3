#include "ns3/header.h"
#include "ns3/nstime.h" // Se você quiser incluir o tempo

namespace ns3 {

class CustomUdpHeader : public Header
{
public:
    // Padrão ns-3
    static TypeId GetTypeId();
    virtual TypeId GetInstanceTypeId() const;
    virtual void Print(std::ostream& os) const;
    virtual void Serialize(Buffer::Iterator start) const;
    virtual uint32_t Deserialize(Buffer::Iterator start);
    virtual uint32_t GetSerializedSize() const;

    // Seu campo personalizado
    void SetCustomId(uint32_t id);
    uint32_t GetCustomId() const;

private:
    uint32_t m_customId;
};

} // namespace ns3
