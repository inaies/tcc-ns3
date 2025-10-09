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

int
main(int argc, char* argv[])
{
    LogComponentEnable("Ping", LOG_LEVEL_INFO);
    LogComponentEnable("ThirdScriptExample", LOG_LEVEL_INFO); 

    bool verbose = true;
    // Alterado os valores default para testar com 120 nós
    uint32_t nWifiCsma = 120; // WiFi 3 (nós STAs)
    uint32_t nWifi = 120;     // WiFi 1 e WiFi 2 (nós STAs)
    bool tracing = false;
    
    // Configurações para o grid de mobilidade
    uint32_t gridWidth = 12; // 12 * 10 = 120 STAs

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifiCsma", "Number of STA devices in the new WiFi 3 network", nWifiCsma);
    cmd.AddValue("nWifi", "Number of STA devices in WiFi 1 and 2", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("gridWidth", "Grid width for mobility (must be >= 12 for 120 nodes)", gridWidth);

    cmd.Parse(argc, argv);

    // Permitindo 120 nós (3 * 120 = 360 nós STA + 3 roteadores)
    if (nWifi > 120 || nWifiCsma > 120) 
    {
        std::cout << "Número de nós excede 120 por rede." << std::endl;
        return 1;
    }
    
    // AVISO: A largura mínima deve ser 12 para 120 nós.
    if (nWifi > gridWidth * gridWidth) 
    {
        NS_LOG_WARN("GridWidth pode ser muito pequeno para o número de nós. Tentando continuar...");
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
    // *** SETUP WIFI NODES (1, 2, 3) ***
    // --------------------------------------------------------------------------------

    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(nWifi);

    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(nWifi);

    NodeContainer wifiStaNodes3;
    wifiStaNodes3.Create(nWifiCsma); 
    
    NodeContainer wifiApNode = p2pNodes.Get(0);
    NodeContainer wifiApNode2 = p2pNodes.Get(2);
    NodeContainer wifiApNode3 = p2pNodes.Get(1); 

    // Configuração de canais, PHY, MAC para as 3 redes (mantida)
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

    // WiFi 1 (AP1 - n0)
    NetDeviceContainer staDevices1;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi.Install(phy1, mac, wifiStaNodes1); 

    NetDeviceContainer apDevices1;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices1 = wifi.Install(phy1, mac, wifiApNode); 
    
    // WiFi 2 (AP3 - n2)
    NetDeviceContainer staDevices2;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);

    NetDeviceContainer apDevices2;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    // WiFi 3 (AP2 - n1)
    NetDeviceContainer staDevices3;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);

    NetDeviceContainer apDevices3;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    apDevices3 = wifi.Install(phy3, mac, wifiApNode3); 

    // --------------------------------------------------------------------------------
    // *** MOBILITY (AUMENTADO O TAMANHO DO GRID) ***
    // --------------------------------------------------------------------------------
    MobilityHelper mobility;
    
    // Aumenta o DeltaX e DeltaY para cobrir uma área maior para 120 nós
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0), // Aumentado
                                  "DeltaY", DoubleValue(20.0), // Aumentado
                                  "GridWidth", UintegerValue(gridWidth), 
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-100, 300, -100, 300))); // Aumentado o Bounds
    mobility.Install(wifiStaNodes1);
    mobility.Install(wifiStaNodes2);
    mobility.Install(wifiStaNodes3); 

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiApNode2);
    mobility.Install(wifiApNode3); 

    // --------------------------------------------------------------------------------
    // *** INSTALAÇÃO DAS PILHAS DE ROTEAMENTO ***
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
    
    // --------------------------------------------------------------------------------
    // *** ENDEREÇAMENTO IPv6 ***
    // --------------------------------------------------------------------------------

    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64)); // AP1-AP2
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
    
    // Habilitar Forwarding (Roteamento) nos Roteadores (p2pNodes)
    for (uint32_t i = 0; i < p2pNodes.GetN(); ++i)
    {
        Ptr<Ipv6> ipv6 = p2pNodes.Get(i)->GetObject<Ipv6>();
        // Correção: SetForwarding(0, true) estava errado. Deve ser SetForwarding(true) ou sem o índice.
        ipv6->SetForwarding(true); 
    }

    // --------------------------------------------------------------------------------
    // *** CONFIGURAÇÃO DAS ROTAS ESTÁTICAS (Nós Finais) ***
    // --------------------------------------------------------------------------------

    // 1. Nós WiFi STA (Rede 1) apontam para o AP1 (nó 0)
    Ipv6Address ap1Addr = apInterfaces1.GetAddress(0, 1); 
    for (uint32_t i = 0; i < wifiStaNodes1.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes1.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices1.Get(i));
        sr->SetDefaultRoute(ap1Addr, ifSta);
    }

    // 2. Nós WiFi STA (Rede 2) apontam para o AP3 (nó 2)
    Ipv6Address ap3Addr = apInterfaces2.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes2.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes2.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices2.Get(i));
        sr->SetDefaultRoute(ap3Addr, ifSta);
    }

    // 3. Nós WiFi STA (Rede 3) apontam para o AP2 (nó 1)
    Ipv6Address ap2Addr = apInterfaces3.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes3.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes3.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices3.Get(i));
        sr->SetDefaultRoute(ap2Addr, ifSta);
    }
    
    // --------------------------------------------------------------------------------
    // *** AGENDAMENTO DA FALHA (AP WiFi 1 = Nó 0) ***
    // --------------------------------------------------------------------------------
    
    Ptr<Ipv6> ipv6 = wifiApNode.Get(0)->GetObject<Ipv6>();
    // Obtém o índice da interface Wi-Fi do AP1 (nó 0)
    int32_t ifIndex = ipv6->GetInterfaceForDevice(apDevices1.Get(0)); 
    
    // Agenda a desativação da interface usando o método correto Ipv6::SetDown
    Simulator::Schedule(Seconds(5.0), &Ipv6::SetDown, ipv6, ifIndex);

    NS_LOG_INFO("Interface do AP da WiFi 1 (Node 0) agendada para cair em 5.0s (IfIndex: " << ifIndex << ")");

    // *** PING IPv6: WiFi 3 (Fonte) -> WiFi 2 (Destino) ***
    // O ping agora testa o roteamento passando por AP1 ou AP3/AP2 (o RIPng deve se ajustar)
    // Se o ping for da WiFi 3 (AP2) para WiFi 2 (AP3), a rota principal é AP2 -> AP3.
    // A rota AP2 -> AP1 -> AP3 é a rota de backup.
    
    // Vamos mudar para WiFi 3 (Fonte) -> WiFi 2 (Destino). Endereço do primeiro STA da Rede 2.
    Ipv6Address pingDestination = wifiInterfaces2.GetAddress(0, 1); 
    
    PingHelper ping(pingDestination); 
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(512));
    ping.SetAttribute("Count", UintegerValue(10));

    // Nó Fonte: O primeiro STA da nova rede WiFi 3 (índice 0)
    ApplicationContainer pingApp = ping.Install(wifiStaNodes3.Get(0)); 
    pingApp.Start(Seconds(30.0));
    pingApp.Stop(Seconds(110.0));

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
