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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThirdScriptExample");

// Função adaptada para criar um ListPositionAllocator para N nós
static Ptr<ListPositionAllocator>
CreateGridPositionAllocator (uint32_t nNodes, double spacing, double offsetX, double offsetY)
{
    Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator> ();
    // cols = número de colunas necessário para acomodar nNodes em um quadrado/retângulo
    uint32_t cols = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(nNodes))));
    
    // Garantir que 120 nós usem pelo menos 11x11 (121) ou 12x10 (120)
    if (nNodes > 100 && cols < 11) {
        cols = 11;
    }
    
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

int
main(int argc, char* argv[])
{
    LogComponentEnable("Ping", LOG_LEVEL_INFO);
    LogComponentEnable("ThirdScriptExample", LOG_LEVEL_INFO); 

    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    bool verbose = true;
    uint32_t nWifi = 120; // Alterado o valor padrão para 120
    bool tracing = false;
    
    // Spacing para garantir que a área seja grande o suficiente
    double sta_spacing = 5.0; // Distância entre os nós STA

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifi", "Number of STA devices per WiFi network (up to 120)", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    // Se o usuário especificar um número muito alto, limitamos
    if (nWifi > 120) {
        NS_LOG_WARN("nWifi limitado a 120 dispositivos por rede para esta simulação.");
        nWifi = 120;
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
    // *** CRIAÇÃO DOS NÓS WIFI (NÓS CRIADOS com base em nWifi) ***
    // --------------------------------------------------------------------------------

    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(nWifi);

    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(nWifi);

    NodeContainer wifiStaNodes3;
    wifiStaNodes3.Create(nWifi); 
    
    NodeContainer wifiApNode = p2pNodes.Get(0);
    NodeContainer wifiApNode2 = p2pNodes.Get(2);
    NodeContainer wifiApNode3 = p2pNodes.Get(1); 

    // --------------------------------------------------------------------------------
    // *** SETUP WIFI DEVICES (Canais, PHY, MAC) ***
    // --------------------------------------------------------------------------------
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default(); 
    YansWifiPhyHelper phy1;
    phy1.SetChannel(channel1.Create());

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default(); 
    YansWifiPhyHelper phy2;
    phy2.SetChannel(channel2.Create());

    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default(); 
    YansWifiPhyHelper phy3;
    phy3.SetChannel(channel3.Create());

    WifiMacHelper mac;
    WifiHelper wifi;

    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");
    Ssid ssid3 = Ssid("ns-3-ssid-3"); 

    // WiFi 1 (AP1)
    NetDeviceContainer staDevices1;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi.Install(phy1, mac, wifiStaNodes1); 

    NetDeviceContainer apDevices1;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices1 = wifi.Install(phy1, mac, wifiApNode); 

    // WiFi 2 (AP3)
    NetDeviceContainer staDevices2;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);

    NetDeviceContainer apDevices2;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    // WiFi 3 (AP2)
    NetDeviceContainer staDevices3;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);

    NetDeviceContainer apDevices3;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    apDevices3 = wifi.Install(phy3, mac, wifiApNode3); 

    // --------------------------------------------------------------------------------
    // *** MOBILITY (POSICIONAMENTO AJUSTADO PARA NÓS) ***
    // --------------------------------------------------------------------------------
    
    MobilityHelper mobility;

    // Calcular a dimensão da grid para nWifi nós
    double grid_size = std::ceil(std::sqrt(static_cast<double>(nWifi)));
    double grid_length = grid_size * sta_spacing;
    
    // Offsets de separação grandes para garantir que as redes não se sobreponham
    double offset_separation = 1000.0; 

    Ptr<ListPositionAllocator> alloc1 = CreateGridPositionAllocator (wifiStaNodes1.GetN(), sta_spacing, 0.0, 0.0);
    Ptr<ListPositionAllocator> alloc2 = CreateGridPositionAllocator (wifiStaNodes2.GetN(), sta_spacing, offset_separation, 0.0); // Offset X
    Ptr<ListPositionAllocator> alloc3 = CreateGridPositionAllocator (wifiStaNodes3.GetN(), sta_spacing, 0.0, offset_separation); // Offset Y

    // STAs
    mobility.SetPositionAllocator (alloc1);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes1);

    mobility.SetPositionAllocator (alloc2);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes2);

    mobility.SetPositionAllocator (alloc3);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes3);

    // APs (posicionados perto do centro de suas respectivas grids)
    Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator> ();
    apAlloc->Add (Vector (grid_length / 2.0, grid_length / 2.0, 0.0 ));                             // AP1 (Rede 1)
    apAlloc->Add (Vector (offset_separation + grid_length / 2.0, grid_length / 2.0, 0.0 ));        // AP2 (Rede 2)
    apAlloc->Add (Vector (grid_length / 2.0, offset_separation + grid_length / 2.0, 0.0 ));        // AP3 (Rede 3)

    mobility.SetPositionAllocator (apAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNode);
    mobility.Install (wifiApNode2);
    mobility.Install (wifiApNode3);

    // --------------------------------------------------------------------------------
    // *** INSTALAÇÃO DAS PILHAS DE ROTEAMENTO E ENDEREÇAMENTO ***
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

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64)); ap1ap2 = address.Assign(ap1ap2);
    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64)); wifiInterfaces1 = address.Assign(staDevices1); apInterfaces1 = address.Assign(apDevices1);
    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64)); wifiInterfaces2 = address.Assign(staDevices2); apInterfaces2 = address.Assign(apDevices2);
    address.SetBase(Ipv6Address("2001:7::"), Ipv6Prefix(64)); wifiInterfaces3 = address.Assign(staDevices3); apInterfaces3 = address.Assign(apDevices3);
    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64)); ap1ap3 = address.Assign(ap1ap3);
    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64)); ap2ap3 = address.Assign(ap2ap3);
    
    // Habilitar Forwarding nos Roteadores (n0, n1, n2)
    for (uint32_t i = 0; i < p2pNodes.GetN(); ++i)
    {
        Ptr<Ipv6> ipv6 = p2pNodes.Get(i)->GetObject<Ipv6>();
        ipv6->SetForwarding(true);
    }

    // --------------------------------------------------------------------------------
    // *** CONFIGURAÇÃO DAS ROTAS ESTÁTICAS (LAÇOS CORRIGIDOS) ***
    // --------------------------------------------------------------------------------

    // 1. WiFi 1 (Nós apontam para AP1)
    Ipv6Address ap1Addr = apInterfaces1.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes1.GetN(); i++) // Usa GetN() que é igual a nWifi
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes1.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices1.Get(i));
        sr->SetDefaultRoute(ap1Addr, ifSta);
    }

    // 2. WiFi 2 (Nós apontam para AP3)
    Ipv6Address ap3Addr = apInterfaces2.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes2.GetN(); i++) // Usa GetN()
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes2.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices2.Get(i));
        sr->SetDefaultRoute(ap3Addr, ifSta);
    }

    // 3. WiFi 3 (Nós apontam para AP2)
    Ipv6Address ap2Addr = apInterfaces3.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes3.GetN(); i++) // Usa GetN()
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes3.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices3.Get(i));
        sr->SetDefaultRoute(ap2Addr, ifSta);
    }
    
    // --------------------------------------------------------------------------------
    // *** APLICAÇÃO (UdpEcho) E SINCRONIZAÇÃO ***
    // --------------------------------------------------------------------------------

    // Server on net3 first STA (Node: wifiStaNodes3.Get(0))
    UdpEchoServerHelper echoServer (9);
    ApplicationContainer serverApps = echoServer.Install (wifiStaNodes3.Get(0)); 
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (50.0));

    // Client from net1 STA (Node: wifiStaNodes1.Get(5)) to server on net3
    // O endereço de destino deve ser o endereço da STA em net3
    Ipv6Address serverAddress = wifiInterfaces3.GetAddress(0, 1); 

    UdpEchoClientHelper echoClient (serverAddress, 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (2));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (64));

    ApplicationContainer clientApps1 = echoClient.Install (wifiStaNodes1.Get(5));
    clientApps1.Start (Seconds (2.0));
    clientApps1.Stop (Seconds (50.0));

    Simulator::Stop(Seconds(120.0));

    if (tracing)
    {
        phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        pointToPoint.EnablePcapAll("third");
        phy1.EnablePcap("third", apDevices1.Get(0)); 
        phy3.EnablePcap("third", apDevices3.Get(0), true); 
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
