// third_three_wifi_fixed_ipv6_mobility.cc
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ping-helper.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ripng-helper.h"

#include <cmath>

using namespace ns3;

// ==============================================================================
// 1. CLASSE DA FLAG (SourceIdHeader) - ADICIONADO
// ==============================================================================
class SourceIdHeader : public Header
{
public:
    SourceIdHeader ();
    void SetSourceId (uint8_t id);
    uint8_t GetSourceId () const;

    static TypeId GetTypeId (void);
    TypeId GetInstanceTypeId (void) const override;
    void Print (std::ostream &os) const override;
    void Serialize (Buffer::Iterator start) const override;
    uint32_t Deserialize (Buffer::Iterator start) override;
    uint32_t GetSerializedSize (void) const override;

private:
    uint8_t m_sourceId; 
};

NS_OBJECT_ENSURE_REGISTERED (SourceIdHeader);

SourceIdHeader::SourceIdHeader () : m_sourceId (0) {}

TypeId
SourceIdHeader::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::SourceIdHeader")
        .SetParent<Header> ()
        .AddConstructor<SourceIdHeader> ();
    return tid;
}

TypeId
SourceIdHeader::GetInstanceTypeId (void) const
{
    return GetTypeId ();
}

uint32_t
SourceIdHeader::GetSerializedSize (void) const
{
    return 1; // 1 byte para o NodeId
}

void
SourceIdHeader::Serialize (Buffer::Iterator start) const
{
    start.WriteU8 (m_sourceId);
}

uint32_t
SourceIdHeader::Deserialize (Buffer::Iterator start)
{
    m_sourceId = start.ReadU8 ();
    return GetSerializedSize ();
}

void
SourceIdHeader::Print (std::ostream &os) const
{
    os << "SourceId=" << (uint32_t)m_sourceId;
}

void
SourceIdHeader::SetSourceId (uint8_t id)
{
    m_sourceId = id;
}

uint8_t
SourceIdHeader::GetSourceId (void) const
{
    return m_sourceId;
}

// ==============================================================================
// FUNÇÃO DE ENVIO COM A FLAG (CALLBACK DO EMISSOR) - ADICIONADO
// ==============================================================================
void 
AddHeader (Ptr<const Packet> p, const Address &src, const Address &dst, uint32_t prot, Ptr<Socket> socket)
{
    // Obtém o ID do nó (simples)
    Ptr<Node> senderNode = socket->GetNode();
    uint8_t senderId = senderNode->GetId();
    
    // Cria o cabeçalho e define a flag
    SourceIdHeader header;
    header.SetSourceId(senderId);

    // Cria um novo pacote com o cabeçalho anexado
    Ptr<Packet> newPkt = p->Copy();
    newPkt->AddHeader(header);
    
    // Envia o novo pacote modificado
    socket->Send(newPkt);
}

// ==============================================================================
// FUNÇÃO DE RECEBIMENTO COM A LEITURA DA FLAG (SUBSTITUI PACKETSINK) - ADICIONADO
// ==============================================================================
void 
ReceivePacket (Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    while ((packet = socket->Recv()))
    {
        SourceIdHeader header;
        // Tenta remover o cabeçalho personalizado
        packet->RemoveHeader(header);
        
        // Obtém o ID do remetente
        uint8_t sourceId = header.GetSourceId();

        Ptr<Node> receiverNode = socket->GetNode();
        
        // Imprime o log
        NS_LOG_UNCOND("--- RECEIVED --- Time: " << Simulator::Now().GetSeconds() 
                    << "s, Receiver ID: " << receiverNode->GetId() 
                    << ", Packet from Source ID: " << (uint32_t)sourceId);
    }
}


static Ptr<ListPositionAllocator>
CreateGridPositionAllocator (uint32_t nNodes, double spacing, double offsetX, double offsetY)
{
// ... (Função CreateGridPositionAllocator mantida)
  Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator> ();
  if (nNodes == 0)
    return allocator;
  uint32_t cols = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(nNodes))));
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      uint32_t row = i / cols;
      uint32_t col = i % cols;
      double x = offsetX + col * spacing;
      double y = offsetY + row * spacing;
      allocator->Add (Vector (x, y, 0.0));
    }
  return allocator;
}

NS_LOG_COMPONENT_DEFINE("ThirdScriptExample");

int
main(int argc, char* argv[])
{
    LogComponentEnable("Ping", LOG_LEVEL_INFO);
    LogComponentEnable("ThirdScriptExample", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO); // Habilitando OnOff Log
    // LogComponentEnable("PacketSink", LOG_LEVEL_DEBUG); // Não é mais necessário, pois estamos usando ReceivePacket

    bool verbose = true;
// ... (Restante do cabeçalho do main, Nodes e Devices mantido)
    uint32_t nWifiCsma = 173; 
    uint32_t nWifi = 173;
    bool tracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifiCsma", "Number of STA devices in the new WiFi 3 network", nWifiCsma);
    cmd.AddValue("nWifi", "Number of STA devices in WiFi 1 and 2", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);
// ... (Criação de Nodes, P2P e WiFi Devices mantida)

    // Endereçamento IPv6 (Mantido)
    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64)); 
    Ipv6InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64)); // WiFi1 (AP1)
    Ipv6InterfaceContainer wifiInterfaces1 = address.Assign(staDevices1);
    Ipv6InterfaceContainer apInterfaces1   = address.Assign(apDevices1);

    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64)); // WiFi2 (AP3)
    Ipv6InterfaceContainer wifiInterfaces2 = address.Assign(staDevices2);
    Ipv6InterfaceContainer apInterfaces2   = address.Assign(apDevices2);

    address.SetBase(Ipv6Address("2001:7::"), Ipv6Prefix(64)); // WiFi3 (AP2)
    Ipv6InterfaceContainer wifiInterfaces3 = address.Assign(staDevices3);
    Ipv6InterfaceContainer apInterfaces3   = address.Assign(apDevices3);

    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64)); // AP1-AP3
    Ipv6InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64)); // AP2-AP3
    Ipv6InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);
// ... (Roteamento e Rotas estáticas STAs mantido)


// --------------------------------------------------------------------------------
// APPS: TRÁFEGO SEQUENCIAL COM FLAG
// --------------------------------------------------------------------------------

    // 1. Configuração do Receptor (Sink) no AP2 (n1) - SUBSTITUÍDO
    Ptr<Node> ap2_receptor = wifiApNode2.Get(0); // AP2 (n1)
    uint16_t sinkPort = 9002;
    
    // --- CRIAÇÃO MANUAL DO SOCKET UDP NO RECEPTOR (AP2) ---
    Ptr<UdpSocketFactory> socketFactory = CreateObject<UdpSocketFactory>();
    Ptr<Socket> sinkSocket = socketFactory->CreateSocket();
    sinkSocket->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), sinkPort));
    // Conecta a função de recebimento personalizada (ReceivePacket)
    sinkSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // 2. Configuração do Emissor (OnOff)
    
    Ipv6Address ap2_address = apInterfaces2.GetAddress(0, 1); 

    OnOffHelper onoff("ns3::UdpSocketFactory",
        Address(Inet6SocketAddress(ap2_address, sinkPort)));
    
    onoff.SetAttribute("DataRate", StringValue("100kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1000)); 
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.001]")); 
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=1000]"));

    // 3. Agendamento Sequencial
    double start_offset = 1.0; 
    double interval = 2;     
    
    for (uint32_t i = 0; i < wifiStaNodes2.GetN(); i++)
    {
      // Cria uma instância do OnOffHelper para cada nó
      ApplicationContainer clientApp = onoff.Install(wifiStaNodes2.Get(i));
      
      // OBTÉM O SOCKET E CONECTA O CALLBACK DA FLAG
      Ptr<OnOffApplication> onOffApp = clientApp.Get(0)->GetObject<OnOffApplication>();
      onOffApp->TraceConnectWithoutContext("TxWithAddresses", MakeCallback(&AddHeader));

      // Agenda o início da transmissão do nó 'i'
      clientApp.Start(Seconds(start_offset + i * interval));
      clientApp.Stop(Seconds(start_offset + i * interval + 1.0)); // Roda por 1 segundo apenas
    }


    Simulator::Stop(Seconds(500.0)); 


    // ... (Bloco de Tracing mantido)

    if (tracing)
    {
        // Configuração de rastreamento adaptada para corresponder ao seu último bloco,
        // mas é recomendado usar a configuração expandida (phy1, phy2, phy3)
        pointToPoint.EnablePcapAll("p2p-traffic-ddos");

        phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy2.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy3.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

        phy1.EnablePcap("test3_ap1", apDevices1.Get(0)); // AP1
        phy2.EnablePcap("test3_ap2_rede2", apDevices2.Get(0)); // AP2 (Rede 2)
        phy3.EnablePcap("test3_ap3_rede3", apDevices3.Get(0)); // AP3 (Rede 3)
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
