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
// 1. CLASSE DA FLAG (SourceIdHeader) - MANTIDO
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

// ---
// FUNÇÃO ORIGINAL REMOVIDA:
// void AddHeader (Ptr<const Packet> p, const Address &src, const Address &dst, uint32_t prot, Ptr<Socket> socket)
// ---


// ==============================================================================
// FUNÇÃO DE ENVIO COM A FLAG (CALLBACK DO EMISSOR) - CORRIGIDO
// Conectado ao UdpSocket::Tx (assinatura: void Tx (std::string context, Ptr<const Packet> p, const Address &dst))
// ==============================================================================
void 
SendPacketWithHeader (std::string context, Ptr<const Packet> p, const Address &dst)
{
    // Obtém o Ptr<Socket> a partir do contexto (o objeto que gerou o trace, passado pelo TraceConnect)
    Ptr<Socket> socket = StaticCast<Socket>(Context::GetObject<Object>(context));
    
    // VERIFICAÇÃO DE SEGURANÇA (melhor para debug)
    if (!socket)
    {
        NS_LOG_WARN("Socket not found in context: " << context);
        return;
    }

    // Obtém o ID do nó
    Ptr<Node> senderNode = socket->GetNode();
    uint8_t senderId = senderNode->GetId();
    
    // Cria o cabeçalho e define a flag
    SourceIdHeader header;
    header.SetSourceId(senderId);

    // Cria um novo pacote com o cabeçalho anexado
    Ptr<Packet> newPkt = p->Copy();
    newPkt->AddHeader(header);
    
    // Envia o novo pacote modificado usando o socket (SendTo é necessário pois a conexão já foi feita pelo OnOffApp)
    // O 0 é para flags (não usamos)
    socket->SendTo(newPkt, 0, dst);
}

// ==============================================================================
// FUNÇÃO DE RECEBIMENTO COM A LEITURA DA FLAG (SUBSTITUI PACKETSINK) - MANTIDO
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
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_DEBUG); // Não é mais necessário

    bool verbose = true;
    uint32_t nWifiCsma = 173; 
    uint32_t nWifi = 173;
    bool tracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifiCsma", "Number of STA devices in the new WiFi 3 network", nWifiCsma);
    cmd.AddValue("nWifi", "Number of STA devices in WiFi 1 and 2", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    NodeContainer p2pNodes;
    p2pNodes.Create(3); // n0=AP1, n1=AP2/WiFi3 AP, n2=AP3

    // Ponto-a-Ponto
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2, ap1ap3, ap2ap3;
    ap1ap2 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(1));
    ap1ap3 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));
    ap2ap3 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));

    // --------------------------------------------------------------------------------
    // WiFi nodes
    // --------------------------------------------------------------------------------

    NodeContainer wifiStaNodes1; wifiStaNodes1.Create(nWifi);
    NodeContainer wifiStaNodes2; wifiStaNodes2.Create(nWifi);
    NodeContainer wifiStaNodes3; wifiStaNodes3.Create(nWifiCsma);

    NodeContainer wifiApNode  = p2pNodes.Get(0); // AP1
    NodeContainer wifiApNode2 = p2pNodes.Get(1); // AP3 (WiFi2)
    NodeContainer wifiApNode3 = p2pNodes.Get(2); // AP2 (WiFi3)

    // PHY/MAC (idem ao original)
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy1; phy1.SetChannel(channel1.Create());
    phy1.Set("ChannelSettings", StringValue("{36, 0, BAND_5GHZ, 0}"));

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy2; phy2.SetChannel(channel2.Create());
    phy2.Set("ChannelSettings", StringValue("{40, 0, BAND_5GHZ, 0}"));

    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy3; phy3.SetChannel(channel3.Create());
    phy3.Set("ChannelSettings", StringValue("{44, 0, BAND_5GHZ, 0}"));

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    // wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");

    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");
    Ssid ssid3 = Ssid("ns-3-ssid-3");

    // WiFi 1 (AP1)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy1, mac, wifiStaNodes1);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifi.Install(phy1, mac, wifiApNode);

    // WiFi 2 (AP3)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    // WiFi 3 (AP2)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    NetDeviceContainer apDevices3 = wifi.Install(phy3, mac, wifiApNode3);

    // --------------------------------------------------------------------------------
    // Mobilidade
    // --------------------------------------------------------------------------------

    MobilityHelper mobility;

    // Parâmetros: espaçamento entre nós na grade e offsets para separar redes
    double spacing = 5.0;    // distância entre STAs (m)
    double offsetCell = 75.0; // distância entre centros das células

    // Cria alocadores de posição separados para cada rede
    Ptr<ListPositionAllocator> allocWifi1 = CreateGridPositionAllocator (nWifi, spacing, 0.0, 0.0);
    Ptr<ListPositionAllocator> allocWifi2 = CreateGridPositionAllocator (nWifi, spacing, 0.0, offsetCell);
    Ptr<ListPositionAllocator> allocWifi3 = CreateGridPositionAllocator (nWifiCsma, spacing, offsetCell, 0.0);

    // Instala posições e modelo constante nos STAs
    mobility.SetPositionAllocator (allocWifi1);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes1);

    mobility.SetPositionAllocator (allocWifi2);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes2);

    mobility.SetPositionAllocator (allocWifi3);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes3);

    // Coloca APs perto do centro de cada grade
    Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator> ();

    // Centro aproximado de cada grade
    uint32_t cols1 = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(std::max<uint32_t>(1, nWifi)))));
    uint32_t cols3 = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(std::max<uint32_t>(1, nWifiCsma)))));

    // AP1 center
    double ap1x = (cols1 * spacing) / 2.0;
    double ap1y = (cols1 * spacing) / 2.0;
    apAlloc->Add (Vector (ap1x, ap1y, 0.0));

    // AP3 (for WiFi2) center - note: this AP is at offsetCell in Y
    double ap3x = (cols1 * spacing) / 2.0;
    double ap3y = offsetCell + (cols1 * spacing) / 2.0;
    apAlloc->Add (Vector (ap3x, ap3y, 0.0));
    
    // AP2 (for WiFi3) center - offset in X
    double ap2x = offsetCell + (cols3 * spacing) / 2.0;
    double ap2y = (cols3 * spacing) / 2.0;
    apAlloc->Add (Vector (ap2x, ap2y, 0.0)); // O p2pNodes.Get(1) é o AP2 no seu mapeamento original

    mobility.SetPositionAllocator (apAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

    // Instala nos APs na ordem de alocação (AP1, AP3, AP2)
    mobility.Install (wifiApNode);  // AP1 (p2pNodes.Get(0)) -> primeira posição
    mobility.Install (wifiApNode2); // AP3 (p2pNodes.Get(2)) -> terceira posição
    mobility.Install (wifiApNode3); // AP2 (p2pNodes.Get(1)) -> segunda posição

    // --------------------------------------------------------------------------------
    // Pilhas e Endereçamento
    // --------------------------------------------------------------------------------

    // 1. Roteadores (n0, n1, n2) usam RIPng
    RipNgHelper ripNg;
    Ipv6ListRoutingHelper listRh;
    listRh.Add(ripNg, 0);

    InternetStackHelper routerStack;
    routerStack.SetRoutingHelper(listRh);
    routerStack.Install(p2pNodes);

    // 2. Nós Finais (STAs das três redes) usam Ipv6StaticRouting
    Ipv6StaticRoutingHelper ipv6StaticRouting;
    InternetStackHelper staStack;
    staStack.SetRoutingHelper(ipv6StaticRouting);

    staStack.Install(wifiStaNodes1);
    staStack.Install(wifiStaNodes2);
    staStack.Install(wifiStaNodes3);

    // Endereçamento IPv6
    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64)); 
    Ipv6InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64)); // WiFi1 (AP1)
    Ipv6InterfaceContainer wifiInterfaces1 = address.Assign(staDevices1);
    Ipv6InterfaceContainer apInterfaces1   = address.Assign(apDevices1);

    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64)); // WiFi2 (AP3)
    Ipv6InterfaceContainer wifiInterfaces2 = address.Assign(staDevices2);
    Ipv6InterfaceContainer apInterfaces2   = address.Assign(apDevices2);

    address.SetBase(Ipv6Address("2001:7::"), Ipv6Prefix(64)); // WiFi3 (AP2)
    Ipv6InterfaceContainer wifiInterfaces3 = address.Assign(staDevices3);
    Ipv6InterfaceContainer apInterfaces3   = address.Assign(apDevices3);

    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64)); // AP1-AP3
    Ipv6InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64)); // AP2-AP3
    Ipv6InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);
    
    // Configuração de Rotas Estáticas (Necessário para STAs)
    // Para cada STA em cada rede, defina a rota padrão para o AP (seu gateway)
    for (uint32_t i = 0; i < nWifi; ++i)
    {
        Ptr<Ipv6StaticRouting> staticRouting = 
            ipv6StaticRouting.GetRouting<Ipv6StaticRouting>(wifiStaNodes1.Get(i)->GetObject<Ipv6>());
        staticRouting->SetDefaultRoute(apInterfaces1.GetAddress(0, 1), 
                                      wifiStaNodes1.Get(i)->GetDevice(0)->IfIndex());
    }

    for (uint32_t i = 0; i < nWifi; ++i)
    {
        Ptr<Ipv6StaticRouting> staticRouting = 
            ipv6StaticRouting.GetRouting<Ipv6StaticRouting>(wifiStaNodes2.Get(i)->GetObject<Ipv6>());
        staticRouting->SetDefaultRoute(apInterfaces2.GetAddress(0, 1), 
                                      wifiStaNodes2.Get(i)->GetDevice(0)->IfIndex());
    }

    for (uint32_t i = 0; i < nWifiCsma; ++i)
    {
        Ptr<Ipv6StaticRouting> staticRouting = 
            ipv6StaticRouting.GetRouting<Ipv6StaticRouting>(wifiStaNodes3.Get(i)->GetObject<Ipv6>());
        staticRouting->SetDefaultRoute(apInterfaces3.GetAddress(0, 1), 
                                      wifiStaNodes3.Get(i)->GetDevice(0)->IfIndex());
    }


// --------------------------------------------------------------------------------
// APPS: TRÁFEGO SEQUENCIAL COM FLAG - CORRIGIDO
// --------------------------------------------------------------------------------

    Ptr<Node> ap2_receptor = wifiApNode2.Get(0); 
    uint16_t sinkPort = 9002;

    Ptr<Ipv6> ipv6 = ap2_receptor->GetObject<Ipv6>();

    // Socket Sink (Receptor)
    Ptr<Socket> sinkSocket = Socket::CreateSocket (ipv6, UdpSocketFactory::GetTypeId());

    sinkSocket->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), sinkPort));
    sinkSocket->SetRecvCallback(MakeCallback(&ReceivePacket));
    
    // Endereço de destino (AP3 - receptor)
    Ipv6Address ap2_address = apInterfaces2.GetAddress(0, 1); 

    // OnOff Helper (Emissor)
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
      
      // ** CORREÇÃO DO ERRO DE COMPILAÇÃO: **
      // 1. Obtém o socket criado pelo OnOffApplication
      Ptr<OnOffApplication> onOffApp = clientApp.Get(0)->GetObject<OnOffApplication>();
      Ptr<Socket> clientSocket = onOffApp->GetSocket();

      // 2. Conecta ao Trace Source "Tx" do socket.
      // O 'context' (string) deve ser usado para passar o Ptr<Socket> para a função de callback.
      // O ns-3 armazena o Ptr do socket no contexto para nós.
      clientSocket->TraceConnect("Tx", "MySendContext", MakeCallback(&SendPacketWithHeader));


      // Agenda o início da transmissão do nó 'i'
      clientApp.Start(Seconds(start_offset + i * interval));
      clientApp.Stop(Seconds(start_offset + i * interval + 1.0)); // Roda por 1 segundo apenas
    }


    Simulator::Stop(Seconds(500.0)); 


    // ... (Bloco de Tracing mantido)

    if (tracing)
    {
        pointToPoint.EnablePcapAll("p2p-traffic-ddos");

        phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy2.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy3.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

        phy1.EnablePcap("test4_ap1", apDevices1.Get(0)); // AP1
        phy2.EnablePcap("test4_ap3_rede2", apDevices2.Get(0)); // AP3 (Rede 2)
        phy3.EnablePcap("test4_ap2_rede3", apDevices3.Get(0)); // AP2 (Rede 3)
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
