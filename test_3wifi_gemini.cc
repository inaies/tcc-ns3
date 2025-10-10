#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h" // Mantido por compatibilidade
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

int main(int argc, char* argv[])
{
    LogComponentEnable("Ping", LOG_LEVEL_INFO);
    LogComponentEnable("ThirdScriptExample", LOG_LEVEL_INFO);

    bool verbose = true;
    uint32_t nWifiCsma = 120; // Antes: 3 -> Agora 120 STAs na WiFi 3
    uint32_t nWifi = 120;     // Antes: 3 -> Agora 120 STAs nas WiFi 1 e 2
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifiCsma", "Number of STA devices in the new WiFi 3 network", nWifiCsma);
    cmd.AddValue("nWifi", "Number of STA devices in WiFi 1 and 2", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    NodeContainer p2pNodes;
    p2pNodes.Create(3); // n0=AP1, n1=AP2/WiFi3 AP, n2=AP3

    // --- Ponto-a-Ponto ---
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2, ap1ap3, ap2ap3;
    ap1ap2 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(1));
    ap1ap3 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));
    ap2ap3 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));

    // --- Criação das redes Wi-Fi ---
    NodeContainer wifiStaNodes1, wifiStaNodes2, wifiStaNodes3;
    wifiStaNodes1.Create(nWifi);
    wifiStaNodes2.Create(nWifi);
    wifiStaNodes3.Create(nWifiCsma);

    NodeContainer wifiApNode = p2pNodes.Get(0);
    NodeContainer wifiApNode2 = p2pNodes.Get(2);
    NodeContainer wifiApNode3 = p2pNodes.Get(1); // AP da WiFi3

    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();

    YansWifiPhyHelper phy1, phy2, phy3;
    phy1.SetChannel(channel1.Create());
    phy2.SetChannel(channel2.Create());
    phy3.SetChannel(channel3.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");
    Ssid ssid3 = Ssid("ns-3-ssid-3");

    // WiFi 1
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy1, mac, wifiStaNodes1);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifi.Install(phy1, mac, wifiApNode);

    // WiFi 2
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    // WiFi 3
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    NetDeviceContainer apDevices3 = wifi.Install(phy3, mac, wifiApNode3);

    // --- Mobilidade ---
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(5.0),
        "GridWidth", UintegerValue(20), // antes 3 — agora 20 para espalhar 120 nós
        "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(-200, 200, -200, 200)));

    mobility.Install(wifiStaNodes1);
    mobility.Install(wifiStaNodes2);
    mobility.Install(wifiStaNodes3);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiApNode2);
    mobility.Install(wifiApNode3);

    // --- Pilhas e roteamento ---
    RipNgHelper ripNg;
    Ipv6ListRoutingHelper listRh;
    listRh.Add(ripNg, 0);

    InternetStackHelper routerStack;
    routerStack.SetRoutingHelper(listRh);
    routerStack.Install(p2pNodes);

    Ipv6StaticRoutingHelper ipv6StaticRouting;
    InternetStackHelper staStack;
    staStack.SetRoutingHelper(ipv6StaticRouting);
    staStack.Install(wifiStaNodes1);
    staStack.Install(wifiStaNodes2);
    staStack.Install(wifiStaNodes3);

    // --- Endereçamento IPv6 ---
    Ipv6AddressHelper address;
    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer wifiInterfaces1 = address.Assign(staDevices1);
    Ipv6InterfaceContainer apInterfaces1 = address.Assign(apDevices1);

    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer wifiInterfaces2 = address.Assign(staDevices2);
    Ipv6InterfaceContainer apInterfaces2 = address.Assign(apDevices2);

    address.SetBase(Ipv6Address("2001:7::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer wifiInterfaces3 = address.Assign(staDevices3);
    Ipv6InterfaceContainer apInterfaces3 = address.Assign(apDevices3);

    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);

    // Habilitar forwarding
    for (uint32_t i = 0; i < p2pNodes.GetN(); ++i)
    {
        Ptr<Ipv6> ipv6 = p2pNodes.Get(i)->GetObject<Ipv6>();
        ipv6->SetForwarding(0, true);
    }

    // --- Rotas estáticas dos STAs ---
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

    // --- Ping (mantido igual) ---
    Ipv6Address pingDestination = wifiInterfaces2.GetAddress(0, 1);
    PingHelper ping(pingDestination);
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(512));
    ping.SetAttribute("Count", UintegerValue(10));
    ApplicationContainer pingApp = ping.Install(wifiStaNodes1.Get(2));
    pingApp.Start(Seconds(30.0));
    pingApp.Stop(Seconds(110.0));

    Simulator::Stop(Seconds(120.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

