// third_three_wifi_fixed_ipv6_mobility.cc
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

// Define a taxa de ataque (a mesma do código IPv4 original)
#define ATK_RATE "512kb/s"

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
    // Habilitar log para a aplicação de ataque
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);


    bool verbose = true;
    uint32_t nWifiCsma = 173; // nCsma renomeado para nWifiCsma
    uint32_t nWifi = 173;
    uint32_t nAtk = 1; // NOVO: Número de nós atacantes
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifiCsma", "Number of STA devices in the new WiFi 3 network", nWifiCsma);
    cmd.AddValue("nWifi", "Number of STA devices in WiFi 1 and 2", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    if (nWifi > 200) // segurança para grids gigantes
    {
        std::cout << "nWifi muito grande; ajuste o script ou aumente a área." << std::endl;
        return 1;
    }

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
    NodeContainer wifiAtkNode; wifiAtkNode.Create(nAtk); // NOVO: Nó atacante

    NodeContainer wifiApNode  = p2pNodes.Get(0); // AP1
    NodeContainer wifiApNode2 = p2pNodes.Get(2); // AP3
    NodeContainer wifiApNode3 = p2pNodes.Get(1); // AP2 (WiFi3)

    // PHY/MAC (idem ao original)
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    channel1.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    YansWifiPhyHelper phy1; phy1.SetChannel(channel1.Create());
    phy1.Set("ChannelSettings", StringValue("{36, 0, BAND_5GHZ, 0}"));

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    channel2.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    YansWifiPhyHelper phy2; phy2.SetChannel(channel2.Create());
    phy2.Set("ChannelSettings", StringValue("{40, 0, BAND_5GHZ, 0}"));

    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();
    channel3.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
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

    // NOVO: Nó Atacante (Conectado ao AP1)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer atkDevices = wifi.Install(phy1, mac, wifiAtkNode);


    // --------------------------------------------------------------------------------
    // Mobilidade
    // --------------------------------------------------------------------------------

    MobilityHelper mobility;

    double spacing = 5.0;
    double offsetCell = 75.0;

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

    // NOVO: Posição do atacante (junto com os nós da WiFi 1)
    mobility.SetPositionAllocator (allocWifi1); // Reusa o alocador
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    // Coloca o atacante na última posição gerada pelo allocWifi1
    Ptr<ConstantPositionMobilityModel> position = CreateObject<ConstantPositionMobilityModel> ();
    position->SetPosition (Vector (spacing*0.5, spacing*0.5, 0.0)); // Perto do centro do WiFi 1
    wifiAtkNode.Get(0)->AggregateObject(position);

    // Coloca APs perto do centro de cada grade (mantido o código original)
    Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator> ();
    uint32_t cols1 = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(std::max<uint32_t>(1, nWifi)))));
    uint32_t cols3 = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(std::max<uint32_t>(1, nWifiCsma)))));
    double centerOffset = spacing * 0.5;

    double ap1x = (cols1 * spacing) / 2.0;
    double ap1y = (cols1 * spacing) / 2.0;
    apAlloc->Add (Vector (ap1x, ap1y, 0.0));

    double ap2x = offsetCell + (cols3 * spacing) / 2.0;
    double ap2y = (cols3 * spacing) / 2.0;
    apAlloc->Add (Vector (ap2x, ap2y, 0.0));

    double ap3x = (cols1 * spacing) / 2.0;
    double ap3y = offsetCell + (cols1 * spacing) / 2.0;
    apAlloc->Add (Vector (ap3x, ap3y, 0.0));

    mobility.SetPositionAllocator (apAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

    mobility.Install (wifiApNode);  // AP1 -> first position
    mobility.Install (wifiApNode3); // AP2 -> second position
    mobility.Install (wifiApNode2); // AP3 -> third position

    // --------------------------------------------------------------------------------
    // Pilhas e Endereçamento IPv6
    // --------------------------------------------------------------------------------

    // 1. Roteadores (n0, n1, n2) usam RIPng
    RipNgHelper ripNg;
    Ipv6ListRoutingHelper listRh;
    listRh.Add(ripNg, 0);

    InternetStackHelper routerStack;
    routerStack.SetRoutingHelper(listRh);
    routerStack.Install(p2pNodes); // n0, n1, n2

    // 2. Nós Finais (STAs das três redes + atacante) usam Ipv6StaticRouting
    Ipv6StaticRoutingHelper ipv6StaticRouting;
    InternetStackHelper staStack;
    staStack.SetRoutingHelper(ipv6StaticRouting);

    staStack.Install(wifiStaNodes1);
    staStack.Install(wifiStaNodes2);
    staStack.Install(wifiStaNodes3);
    staStack.Install(wifiAtkNode); // NOVO: Instalar pilha no nó atacante

    // Endereçamento IPv6
    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64)); // AP1-AP2
    Ipv6InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64)); // WiFi1 (AP1)
    Ipv6InterfaceContainer wifiInterfaces1 = address.Assign(staDevices1);
    Ipv6InterfaceContainer apInterfaces1   = address.Assign(apDevices1);
    // NOVO: Atribui endereço IPv6 ao atacante (na sub-rede WiFi 1)
    Ipv6InterfaceContainer atkInterfaces   = address.Assign(atkDevices);


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

    // Habilitar Forwarding (Roteamento) nos Roteadores (p2pNodes)
    for (uint32_t i = 0; i < p2pNodes.GetN(); ++i)
    {
        Ptr<Ipv6> ipv6 = p2pNodes.Get(i)->GetObject<Ipv6>();
        ipv6->SetForwarding(0, true);
    }

    // Rotas estáticas nos STAs (idêntico ao seu original)
    Ipv6Address ap1Addr = apInterfaces1.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes1.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes1.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices1.Get(i));
        sr->SetDefaultRoute(ap1Addr, ifSta);
    }

    Ipv6Address ap3Addr = apInterfaces2.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes2.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes2.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices2.Get(i));
        sr->SetDefaultRoute(ap3Addr, ifSta);
    }

    Ipv6Address ap2Addr = apInterfaces3.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes3.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes3.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices3.Get(i));
        sr->SetDefaultRoute(ap2Addr, ifSta);
    }

    // NOVO: Rota estática no atacante (aponta para AP1)
    {
        Ptr<Ipv6> ipv6 = wifiAtkNode.Get(0)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(atkDevices.Get(0));
        sr->SetDefaultRoute(ap1Addr, ifSta); // Rota padrão via AP1
    }

    // --------------------------------------------------------------------------------
    // Aplicações
    // --------------------------------------------------------------------------------

    // Apps de teste (servidor mantido no primeiro nó WiFi 3)
    Ipv6Address targetAddr = wifiInterfaces3.GetAddress (0, 1); // Alvo: 1º STA da WiFi 3
    uint16_t targetPort = 9;

    UdpEchoServerHelper echoServer (targetPort);
    ApplicationContainer serverApps = echoServer.Install (wifiStaNodes3.Get (0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (300.0));

    UdpEchoClientHelper echoClient (targetAddr, targetPort);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (2));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (64));

    ApplicationContainer clientApps1 = echoClient.Install (wifiStaNodes1.Get (2));
    clientApps1.Start (Seconds (10.0));
    clientApps1.Stop (Seconds (300.0));

    // NOVO: Implementação do ataque de inundação UDP (DoS)

    // 1. Aplicação Sink (Alvo do Tráfego de Ataque)
    uint16_t atkPort = 9001;
    PacketSinkHelper udpsink_h("ns3::UdpSocketFactory",
                                Address(Inet6SocketAddress(Ipv6Address::GetAny(), atkPort)));
    // O alvo será o mesmo nó que roda o UdpEchoServer (1º STA da WiFi 3)
    ApplicationContainer udpsink_App_c = udpsink_h.Install(wifiStaNodes3.Get(0));
    udpsink_App_c.Start(Seconds(1.0));
    udpsink_App_c.Stop(Seconds(300.0));


    // 2. Aplicação OnOff (Cliente Atacante)
    OnOffHelper atk_client_h("ns3::UdpSocketFactory",
                                Address(Inet6SocketAddress(targetAddr, atkPort))); // Endereço IPv6 do alvo
    atk_client_h.SetConstantRate(DataRate(ATK_RATE));
    atk_client_h.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=295]")); // Roda por 295s
    atk_client_h.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    // Ajusta tamanho do pacote para maximizar taxa de pacotes (ex: 512 bytes)
    atk_client_h.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer atk_apps_c = atk_client_h.Install(wifiAtkNode);
    atk_apps_c.Start(Seconds(5.0)); // Começa o ataque 5s após o início
    atk_apps_c.Stop(Seconds(300.0)); // Para no fim da simulação

    // --------------------------------------------------------------------------------
    // Simulação
    // --------------------------------------------------------------------------------

    Simulator::Stop(Seconds(300.0));

    if (tracing)
    {
        phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        pointToPoint.EnablePcapAll("third");
        phy1.EnablePcap("third", apDevices1.Get(0)); // AP1
        phy3.EnablePcap("third", apDevices3.Get(0), true); // AP2
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

