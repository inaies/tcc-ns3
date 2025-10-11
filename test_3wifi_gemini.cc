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

NS_LOG_COMPONENT_DEFINE("ThirdScriptExampleImproved");

int
main(int argc, char* argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("ThirdScriptExampleImproved", LOG_LEVEL_INFO);

    bool verbose = true;
    uint32_t nWifiCsma = 3; // agora maior por padrão
    uint32_t nWifi = 3;
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifiCsma", "Number of STA devices in the new WiFi 3 network", nWifiCsma);
    cmd.AddValue("nWifi", "Number of STA devices in WiFi 1 and 2", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    if (nWifi > 200)
    {
        std::cout << "nWifi too large; keep it reasonable for a single run" << std::endl;
        return 1;
    }

    NodeContainer p2pNodes;
    p2pNodes.Create(3); // n0=AP1, n1=AP2/WiFi3 AP, n2=AP3

    // Ponto-a-Ponto
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2, ap1ap3, ap2ap3;
    ap1ap2 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(1));
    ap1ap3 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));
    ap2ap3 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));

    // --------------------------------------------------------------------------------
    // *** Redes WiFi (3 SSIDs) ***
    // --------------------------------------------------------------------------------

    NodeContainer wifiStaNodes1; wifiStaNodes1.Create(nWifi);
    NodeContainer wifiStaNodes2; wifiStaNodes2.Create(nWifi);

    // AP nodes are p2pNodes elements
    NodeContainer wifiApNode = NodeContainer(p2pNodes.Get(0));
    NodeContainer wifiApNode2 = NodeContainer(p2pNodes.Get(2));
    NodeContainer wifiApNode3 = NodeContainer(p2pNodes.Get(1));

    NodeContainer wifiStaNodes3; wifiStaNodes3.Create(nWifiCsma);

    // 2. Configuração de canais, PHY, MAC para as 3 redes
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    channel1.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
    channel1.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    YansWifiPhyHelper phy1;
    phy1.SetChannel(channel1.Create());

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    channel2.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
    channel2.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    YansWifiPhyHelper phy2;
    phy2.SetChannel(channel2.Create());

    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();
    channel3.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
    channel3.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    YansWifiPhyHelper phy3;
    phy3.SetChannel(channel3.Create());

    // Aumentar potência Tx e sensibilidade para melhorar alcance e robustez
    phy1.Set("TxPowerStart", DoubleValue(20.0));
    phy1.Set("TxPowerEnd", DoubleValue(20.0));
    phy1.Set("RxGain", DoubleValue(5.0));
    phy1.Set("RxNoiseFigure", DoubleValue(7.0));
    Config::SetDefault("ns3::WifiPhy::CcaEdThreshold", DoubleValue(-99.0));
    Config::SetDefault("ns3::WifiPhy::RxSensitivity", DoubleValue(-96.0));

    phy2.Set("TxPowerStart", DoubleValue(20.0));
    phy2.Set("TxPowerEnd", DoubleValue(20.0));
    phy2.Set("RxGain", DoubleValue(5.0));
    phy2.Set("RxNoiseFigure", DoubleValue(7.0));
    Config::SetDefault("ns3::WifiPhy::CcaEdThreshold", DoubleValue(-99.0));
    Config::SetDefault("ns3::WifiPhy::RxSensitivity", DoubleValue(-96.0));

    phy3.Set("TxPowerStart", DoubleValue(20.0));
    phy3.Set("TxPowerEnd", DoubleValue(20.0));
    phy3.Set("RxGain", DoubleValue(5.0));
    phy3.Set("RxNoiseFigure", DoubleValue(7.0));
    Config::SetDefault("ns3::WifiPhy::CcaEdThreshold", DoubleValue(-99.0));
    Config::SetDefault("ns3::WifiPhy::RxSensitivity", DoubleValue(-96.0));

    // Isolar canais entre as redes (1, 6, 11 são não sobrepostos em 2.4GHz)
    // Configure channel number via WifiPhy's ChannelNumber attribute when available
    // Nem todas as versões do ns-3 permitem definir ChannelNumber via Set() diretamente no helper.
    // Se precisar controlar canais, defina canais separados criando YansWifiPhy e YansWifiChannel separados
    // para cada rede (já fazemos isso) e, se suportado, use o atributo ChannelNumber no phy criado.
    // Mantemos os canais separados pela criação de objetos de canal distintos acima.

    // Wifi helper: usar 802.11g e ConstantRate para reduzir tempo de ocupação do canal
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("ErpOfdmRate24Mbps"),
                                 "ControlMode", StringValue("ErpOfdmRate6Mbps"));

    // Habilitar RTS/CTS para tráfego maior (ajusta threshold)
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(200));

    WifiMacHelper mac;

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

    // Mobilidade: aumentar espaçamento físico e grid width
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));

    // mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
    //                           "Bounds", RectangleValue(Rectangle(-500, 500, -500, 500)));
    
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes1);
    mobility.Install(wifiStaNodes2);
    mobility.Install(wifiStaNodes3);
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
    routerStack.Install(p2pNodes); // n0, n1, n2

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
        // interface 0 geralmente é loopback, por isso setamos forwarding em todas as interfaces válidas
        for (uint32_t j = 0; j < ipv6->GetNInterfaces(); ++j)
        {
            ipv6->SetForwarding(j, true);
        }
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
    // *** APLICACOES: UDP ECHO (exemplo de carga) ***
    // --------------------------------------------------------------------------------

    // Server: vamos instalar em um STA da rede 3 (primeiro STA)
    UdpEchoServerHelper echoServer (9);
    ApplicationContainer serverApps = echoServer.Install (wifiStaNodes3.Get (0));
    serverApps.Start (Seconds (12.0));
    serverApps.Stop (Seconds (190.0));

    // Clientes: alguns STAs nas redes 1 e 2 realizarão envios espaçados
    // Client targeting server's IPv6
    Ipv6Address serverAddr = wifiInterfaces3.GetAddress(0, 1);
    UdpEchoClientHelper echoClient (serverAddr, 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (2));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (2.0))); // espaçar para reduzir congestionamento
    echoClient.SetAttribute ("PacketSize", UintegerValue (128));

    // instalar vários clientes distribuídos
    ApplicationContainer clientApps1, clientApps2;
    uint32_t clientsPerNet = std::min<uint32_t>(wifiStaNodes1.GetN(), 20);
    for (uint32_t i = 0; i < clientsPerNet; ++i)
    {
        Ptr<Node> node = wifiStaNodes1.Get(i);
        clientApps1.Add(echoClient.Install(node));
    }
    // clientsPerNet = std::min<uint32_t>(wifiStaNodes2.GetN(), 20);
    // for (uint32_t i = 0; i < clientsPerNet; ++i)
    // {
    //     Ptr<Node> node = wifiStaNodes2.Get(i);
    //     clientApps2.Add(echoClient.Install(node));
    // }

    clientApps1.Start(Seconds(15.0));
    // clientApps1.Stop(Seconds(190.0));
    clientApps2.Start(Seconds(16.0));
    // clientApps2.Stop(Seconds(190.0));

    Simulator::Stop(Seconds(200.0));

    if (tracing)
    {
        phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        pointToPoint.EnablePcapAll("third_improved");
        phy1.EnablePcap("third_improved", apDevices1.Get(0));
        phy3.EnablePcap("third_improved", apDevices3.Get(0), true);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

