#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h" // Mantido por compatibilidade de includes, mas CSMA removido
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ping-helper.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ripng-helper.h"

#include <cmath>
#include <iostream>
#include <string>

using namespace ns3;

static Ptr<ListPositionAllocator>
CreateGridPositionAllocator (uint32_t nNodes, double spacing, double offsetX, double offsetY)
{
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

    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO); // Adicionado para verificação
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO); // Adicionado para verificação

    bool verbose = true;
    uint32_t nWifiCsma = 173; // nCsma renomeado para nWifiCsma
    uint32_t nWifi = 173;
    bool tracing = true;
    double simTime = 60.0; // Tempo total da simulação

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifiCsma", "Number of STA devices in the new WiFi 3 network", nWifiCsma);
    cmd.AddValue("nWifi", "Number of STA devices in WiFi 1 and 2", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("simTime", "Simulation total time", simTime);

    cmd.Parse(argc, argv);

    if (nWifi > 200) // segurança para grids gigantes
    {
        std::cout << "nWifi muito grande; ajuste o script ou aumente a área." << std::endl;
        return 1;
    }

    NodeContainer p2pNodes;
    p2pNodes.Create(3); // n0=AP1, n1=AP2/WiFi2 AP, n2=AP3/WiFi3 AP

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

    NodeContainer wifiApNode  = p2pNodes.Get(0); // AP1
    NodeContainer wifiApNode2 = p2pNodes.Get(1); // AP2
    NodeContainer wifiApNode3 = p2pNodes.Get(2); // AP3 (WiFi3)

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

    // WiFi 2 (AP2)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    // WiFi 3 (AP3)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    NetDeviceContainer apDevices3 = wifi.Install(phy3, mac, wifiApNode3);

    // --------------------------------------------------------------------------------
    // Mobilidade ADAPTADA para aumentar capacidade (isolar células e controlar densidade)
    // --------------------------------------------------------------------------------

    MobilityHelper mobility;

    // Parâmetros: espaçamento entre nós na grade e offsets para separar redes
    double spacing = 5.0;    // distância entre STAs (m). Ajuste para maior densidade se quiser mais nós por área.
    double offsetCell = 75.0; // distância entre centros das células -> isola co-canal interference

    // Cria alocadores de posição separados para cada rede (mantém as redes fisicamente separadas)
    Ptr<ListPositionAllocator> allocWifi1 = CreateGridPositionAllocator (nWifi, spacing, 0.0, 0.0);
    Ptr<ListPositionAllocator> allocWifi2 = CreateGridPositionAllocator (nWifi, spacing, 0.0, offsetCell);    // deslocada em Y
    Ptr<ListPositionAllocator> allocWifi3 = CreateGridPositionAllocator (nWifiCsma, spacing, offsetCell, 0.0);  // deslocada em X

    // Instala posições e modelo constante nos STAs (posições fixas na grade)
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

    // Centro aproximado de cada grade (calcula colunas a partir do número de nós)
    uint32_t cols1 = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(std::max<uint32_t>(1, nWifi)))));
    uint32_t cols3 = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(std::max<uint32_t>(1, nWifiCsma)))));

    // AP1 center (WiFi1)
    double ap1x = (cols1 * spacing) / 2.0;
    double ap1y = (cols1 * spacing) / 2.0;
    apAlloc->Add (Vector (ap1x, ap1y, 0.0));

    // AP2 center (WiFi2) - offset in Y
    double ap2x = (cols1 * spacing) / 2.0;
    double ap2y = offsetCell + (cols1 * spacing) / 2.0;
    apAlloc->Add (Vector (ap2x, ap2y, 0.0));
    
    // AP3 center (WiFi3) - offset in X
    double ap3x = offsetCell + (cols3 * spacing) / 2.0;
    double ap3y = (cols3 * spacing) / 2.0;
    apAlloc->Add (Vector (ap3x, ap3y, 0.0));

    mobility.SetPositionAllocator (apAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

    // Instala os APs nas posições calculadas.
    mobility.Install (wifiApNode);  // AP1 -> primeira posição
    mobility.Install (wifiApNode2); // AP2 -> segunda posição
    mobility.Install (wifiApNode3); // AP3 -> terceira posição

    // --------------------------------------------------------------------------------
    // Pilhas, Endereçamento e Roteamento
    // --------------------------------------------------------------------------------

    // 1. Roteadores (n0, n1, n2) usam RIPng
    RipNgHelper ripNg;
    Ipv6ListRoutingHelper listRh;
    listRh.Add(ripNg, 0);

    InternetStackHelper routerStack;
    routerStack.SetRoutingHelper(listRh);
    routerStack.Install(p2pNodes); // n0, n1, n2

    // 2. Nós Finais (STAs das três redes) usam Ipv6StaticRouting
    Ipv6StaticRoutingHelper ipv6StaticRouting;
    InternetStackHelper staStack;
    staStack.SetRoutingHelper(ipv6StaticRouting);

    staStack.Install(wifiStaNodes1);
    staStack.Install(wifiStaNodes2);
    staStack.Install(wifiStaNodes3);

    // Endereçamento IPv6
    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64)); // AP1-AP2 (P2P)
    Ipv6InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64)); // WiFi1 (AP1)
    Ipv6InterfaceContainer wifiInterfaces1 = address.Assign(staDevices1);
    Ipv6InterfaceContainer apInterfaces1   = address.Assign(apDevices1);

    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64)); // WiFi2 (AP2)
    Ipv6InterfaceContainer wifiInterfaces2 = address.Assign(staDevices2);
    Ipv6InterfaceContainer apInterfaces2   = address.Assign(apDevices2);

    address.SetBase(Ipv6Address("2001:7::"), Ipv6Prefix(64)); // WiFi3 (AP3)
    Ipv6InterfaceContainer wifiInterfaces3 = address.Assign(staDevices3);
    Ipv6InterfaceContainer apInterfaces3   = address.Assign(apDevices3);

    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64)); // AP1-AP3 (P2P)
    Ipv6InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64)); // AP2-AP3 (P2P)
    Ipv6InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);

    // Habilitar Forwarding (Roteamento) nos Roteadores (p2pNodes)
    for (uint32_t i = 0; i < p2pNodes.GetN(); ++i)
    {
        Ptr<Ipv6> ipv6 = p2pNodes.Get(i)->GetObject<Ipv6>();
        ipv6->SetForwarding(0, true);
    }

    // Rotas estáticas nos STAs (Default Route para o seu AP)
    
    // WiFi 1 (AP1)
    Ipv6Address ap1Addr = apInterfaces1.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes1.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes1.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices1.Get(i));
        sr->SetDefaultRoute(ap1Addr, ifSta);
    }

    // WiFi 2 (AP2)
    Ipv6Address ap2Addr = apInterfaces2.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes2.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes2.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices2.Get(i));
        sr->SetDefaultRoute(ap2Addr, ifSta);
    }

    // WiFi 3 (AP3)
    Ipv6Address ap3Addr = apInterfaces3.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes3.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes3.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices3.Get(i));
        sr->SetDefaultRoute(ap3Addr, ifSta);
    }

    // Simulação de mobilidade (SetDown/SetUp da interface AP2)
    Ptr<Ipv6> ipv6 = wifiApNode2.Get(0)->GetObject<Ipv6>();
    int32_t ifIndex = ipv6->GetInterfaceForDevice(apDevices2.Get(0)); 
    Simulator::Schedule(Seconds(30.0), &Ipv6::SetDown, ipv6, ifIndex);
    Simulator::Schedule(Seconds(40.0), &Ipv6::SetUp, ipv6, ifIndex);

    // --------------------------------------------------------------------------------
    // APPS DE TESTE (TRÁFEGO CÍCLICO PERSISTENTE)
    // --------------------------------------------------------------------------------
    
    // PARÂMETROS PARA O TRÁFEGO CÍCLICO
    uint32_t sta_start_index = 61; // Inicia no nó 61 da rede 2
    uint32_t num_sending_stas = wifiStaNodes2.GetN() - sta_start_index;
    double start_offset = 12.0;  // Tempo de início da primeira transmissão
    double interval = 0.05;      // Intervalo entre o início de cada nó

    // Duração total do ciclo sequencial (quando o último nó começa)
    double cycleDuration = num_sending_stas * interval;

    NS_LOG_INFO ("Nós enviando: " << num_sending_stas);
    NS_LOG_INFO ("Duração do ciclo sequencial (OffTime): " << cycleDuration << " segundos.");

    // 1. Configuração do Receptor (Sink) no AP2 (n1)
    Ptr<Node> ap2_receptor = wifiApNode2.Get(0); // AP2 (n1)
    uint16_t sinkPort = 9002;
    
    PacketSinkHelper sinkHelper(
      "ns3::UdpSocketFactory",
      Inet6SocketAddress(Ipv6Address::GetAny(), sinkPort)
    );
    ApplicationContainer sinkApp = sinkHelper.Install(ap2_receptor);
    sinkApp.Start(Seconds(1.5));
    sinkApp.Stop(Seconds(simTime)); // Para no fim da simulação

    // 2. Configuração do Emissor (OnOff)
    
    Ipv6Address ap2_address = apInterfaces2.GetAddress(0, 1); 

    OnOffHelper onoff("ns3::UdpSocketFactory",
        Address(Inet6SocketAddress(ap2_address, sinkPort)));
    
    // Taxa baixa para garantir que o AP possa receber (100kbps)
    onoff.SetAttribute("DataRate", StringValue("100kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1000)); // Tamanho do pacote em bytes
    
    // OnTime: Tempo para enviar UM pacote (valor deve ser pequeno, o que importa é a taxa e o PacketSize)
    // O valor constante é arbitrário, desde que maior que zero, mas muito curto.
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.001]")); 
    
    // OffTime (A CHAVE): Define o tempo de espera antes do nó tentar enviar o próximo pacote.
    // É igual à duração do ciclo completo, garantindo que o envio recomece apenas
    // após o último nó ter tido a sua vez.
    std::string offTimeStr = "ns3::ConstantRandomVariable[Constant=" + std::to_string(cycleDuration) + "]";
    onoff.SetAttribute("OffTime", StringValue(offTimeStr));

    // 3. Agendamento Sequencial
    // Agenda o início sequencial de cada nó
    for (uint32_t i = sta_start_index; i < wifiStaNodes2.GetN(); i++)
    {
        Ptr<Node> clientNode = wifiStaNodes2.Get(i);
        ApplicationContainer clientApp = onoff.Install(clientNode);
      
        double startTime = start_offset + (i - sta_start_index) * interval;
        
        clientApp.Start(Seconds(startTime));
        // A aplicação para apenas no final da simulação, permitindo ciclos contínuos
        clientApp.Stop(Seconds(simTime)); 
    }

    Simulator::Stop(Seconds(simTime));

    if (tracing)
    {
        pointToPoint.EnablePcapAll("p2p-test3");

        phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy2.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy3.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

        phy1.EnablePcap("test3_ap1", apDevices1.Get(0)); // AP1
        phy2.EnablePcap("test3_ap2", apDevices2.Get(0)); // AP2
        phy3.EnablePcap("test3_ap3", apDevices3.Get(0)); // AP3
        
        // Exemplo de um cliente para ver o tráfego cíclico (STA 61 da rede 2)
        if (nWifi > sta_start_index)
        {
            phy2.EnablePcap("test3_sta61", staDevices2.Get(sta_start_index)); 
        }
    }


    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
