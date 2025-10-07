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
#include "ns3/ripng-helper.h" // Incluído para garantir o RIPng

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThirdScriptExample");

int
main(int argc, char* argv[])
{
    // ... (rest of the initial setup, CommandLine, NodeContainer creation) ...
    LogComponentEnable("Ping", LOG_LEVEL_INFO);

    bool verbose = true;
    uint32_t nCsma = 3;
    uint32_t nWifi = 3;
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
    cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    if (nWifi > 18)
    {
        std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box"
                  << std::endl;
        return 1;
    }

    NodeContainer p2pNodes;
    p2pNodes.Create(3); // n0=AP1, n1=AP2/CSMA Router, n2=AP3

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2, ap1ap3, ap2ap3;
    ap1ap2 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(1));
    ap1ap3 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));
    ap2ap3 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes.Get(1));
    csmaNodes.Create(nCsma);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);

    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(nWifi);

    NodeContainer wifiApNode = p2pNodes.Get(0);
    NodeContainer wifiApNode2 = p2pNodes.Get(2);

    // ... (WiFi setup - channels, PHY, MAC, SSIDs) ...
    
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy1;
    phy1.SetChannel(channel1.Create());

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy2;
    phy2.SetChannel(channel2.Create());

    WifiMacHelper mac;
    WifiHelper wifi;

    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");

    // --- Primeira rede Wi-Fi (AP1 - n0) ---
    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy1, mac, wifiStaNodes);

    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices = wifi.Install(phy1, mac, wifiApNode);

    // --- Segunda rede Wi-Fi (AP3 - n2) ---
    NetDeviceContainer staDevices2;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);

    NetDeviceContainer apDevices2;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    // ... (Mobility setup) ...
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiStaNodes2);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiApNode2);

    // --- Configuração do Roteamento (RIPng) ---
    
    // Adicionar o RIPng apenas nos nós roteadores (n0, n1, n2)
    RipNgHelper ripNgRouter;
    // Note: Usar Ipv6ListRoutingHelper para adicionar RIPng e roteamento estático (caso precise)
    // Mas, como só usaremos RIPng nos roteadores, podemos usar a Ipv6ListRoutingHelper apenas
    // para garantir o RIPng com prioridade.

    Ipv6ListRoutingHelper listRh;
    listRh.Add(ripNgRouter, 0);

    // Pilha IPv6
    InternetStackHelper stack;
    Ipv6StaticRoutingHelper ipv6RoutingHelper;

    // Instalar RIPng nos roteadores (n0, n1, n2)
    stack.SetRoutingHelper(listRh);
    stack.Install(p2pNodes);
    
    // Instalar apenas Static Routing nos nós finais (CSMA "extras" e STAs)
    stack.SetRoutingHelper(ipv6RoutingHelper);
    
    // Nós CSMA extras
    NodeContainer csmaExtras;
    for (uint32_t i = 1; i < csmaNodes.GetN(); ++i)
    {
        csmaExtras.Add(csmaNodes.Get(i));
    }
    stack.Install(csmaExtras); 
    
    // Nós STAs
    stack.Install(wifiStaNodes);
    stack.Install(wifiStaNodes2);

    // --- Endereçamento IPv6 ---
    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64)); // AP1-AP2
    Ipv6InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    address.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64)); // CSMA LAN
    Ipv6InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64)); // WiFi1 (AP1)
    Ipv6InterfaceContainer wifiInterfaces = address.Assign(staDevices);
    Ipv6InterfaceContainer apInterfaces   = address.Assign(apDevices);

    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64)); // WiFi2 (AP3)
    Ipv6InterfaceContainer wifiInterfaces2 = address.Assign(staDevices2);
    Ipv6InterfaceContainer apInterfaces2   = address.Assign(apDevices2);

    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64)); // AP1-AP3
    Ipv6InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64)); // AP2-AP3
    Ipv6InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);
    
    // Habilitar o roteamento (forwarding) em todos os roteadores
    for (uint32_t i = 0; i < p2pNodes.GetN(); ++i)
    {
        Ptr<Ipv6> ipv6 = p2pNodes.Get(i)->GetObject<Ipv6>();
        ipv6->SetForwarding(true);
    }
    
    // --- Configuração das Rotas Estáticas (Nós Finais) ---
    
    // Nós CSMA "extras" (nós 2, 3, 4) apontam para o gateway (nó 1)
    Ipv6Address csmaGatewayAddr = csmaInterfaces.GetAddress(0, 1); 
    for (uint32_t i = 1; i < csmaNodes.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = csmaNodes.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
        // O índice da interface CSMA é 1 (após a interface Loopback)
        sr->SetDefaultRoute(csmaGatewayAddr, 1); 
    }

    // Nós WiFi STA (Rede 1) apontam para o AP1 (nó 0)
    Ipv6Address ap1Addr = apInterfaces.GetAddress(0, 1); 
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
        // O índice da interface WiFi STA é 1 (após a interface Loopback)
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices.Get(i));
        sr->SetDefaultRoute(ap1Addr, ifSta);
    }

    // Nós WiFi STA (Rede 2) apontam para o AP3 (nó 2)
    Ipv6Address ap3Addr = apInterfaces2.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes2.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes2.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
        // O índice da interface WiFi STA é 1 (após a interface Loopback)
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices2.Get(i));
        sr->SetDefaultRoute(ap3Addr, ifSta);
    }

    // O RIPng deve cuidar das rotas entre AP1 (n0), AP2 (n1) e AP3 (n2).
    // As rotas estáticas que você havia colocado foram removidas para o RIPng funcionar.

    // *** PING IPv6 ***
    // Ping do primeiro STA (Rede 1) para o segundo nó CSMA (nó 2 da CSMA LAN)
    PingHelper ping(csmaInterfaces.GetAddress(1, 1)); // Endereço global do segundo nó CSMA
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(512));
    ping.SetAttribute("Count", UintegerValue(10));

    ApplicationContainer pingApp = ping.Install(wifiStaNodes.Get(0));
    pingApp.Start(Seconds(30.0));
    pingApp.Stop(Seconds(110.0));
    
    // Permite que o RIPng converge
    Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(120.0));

    // ... (Tracing and Simulation run/destroy) ...
    if (tracing)
    {
        phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        pointToPoint.EnablePcapAll("third");
        phy1.EnablePcap("third", apDevices.Get(0));
        csma.EnablePcap("third", csmaDevices.Get(0), true);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
