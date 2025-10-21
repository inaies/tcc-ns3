#include "ns3/onoff-application.h"
#include "custom-udp-header.h"

namespace ns3 {

class CustomOnOffApplication : public OnOffApplication
{
public:
    static TypeId GetTypeId();

    CustomOnOffApplication();
    virtual ~CustomOnOffApplication();

    // Novo atributo/setter para o ID
    void SetNodeId(uint32_t nodeId);

protected:
    // Sobrescreve o método SendPacket para adicionar o cabeçalho
    virtual void SendPacket();

private:
    uint32_t m_nodeId;
};

} // namespace ns3
