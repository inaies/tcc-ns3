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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThirdScriptExample");

int main(int argc, char* argv[])
{
    LogComponentEnable("Ping", LOG_LEVEL_INFO);

    uint32_t nCsma = 3;
    uint32_t nWifi = 3;
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
    cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    NodeContainer p2pNodes;
    p2pNodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2, ap1ap3, ap2ap3;
    ap1ap2 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(1));
    ap1ap3 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));
    ap2ap3 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));

    // ---- CSMA 1 (ligado no ap1) ----
    NodeContainer csmaNodes1;
    csmaNodes1.Add(p2pNodes.Get(0));
    csmaNodes1.Create(nCsma);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    NetDeviceContainer csmaDevices1 = csma.Install(csmaNodes1);

    // ---- CSMA 2 (ligado no ap2) ----
    NodeContainer csmaNodes2;
    csmaNodes2.Add(p2pNodes.Get(1));
    csmaNodes2.Create(nCsma);
    NetDeviceContainer csmaDevices2 = csma.Install(csmaNodes2);

    // ---- WiFi (ligado no ap3) ----
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);
    NodeContainer wifiApNode = p2pNodes.Get(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    Ssid ssid = Ssid("ns-3-ssid");

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // Mobilidade
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodes);

    // ---- Pilha IPv6 ----
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    InternetStackHelper stack;
    stack.SetRoutingHelper(ipv6RoutingHelper);
    stack.Install(csmaNodes1);
    stack.Install(csmaNodes2);
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // ---- Endereçamento IPv6 ----
    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    address.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer csmaInterfaces1 = address.Assign(csmaDevices1);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer csmaInterfaces2 = address.Assign(csmaDevices2);

    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIfs = address.Assign(staDevices);
    Ipv6InterfaceContainer apIfs  = address.Assign(apDevices);

    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);

    // ---- Rotas Default nos CSMA ----
    for (uint32_t i = 1; i < csmaNodes1.GetN(); i++) {
        Ptr<Ipv6> ipv6 = csmaNodes1.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(csmaInterfaces1.GetAddress(0,1),
                            ipv6->GetInterfaceForDevice(csmaDevices1.Get(i)));
    }
    for (uint32_t i = 1; i < csmaNodes2.GetN(); i++) {
        Ptr<Ipv6> ipv6 = csmaNodes2.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(csmaInterfaces2.GetAddress(0,1),
                            ipv6->GetInterfaceForDevice(csmaDevices2.Get(i)));
    }
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); i++) {
        Ptr<Ipv6> ipv6 = wifiStaNodes.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(apIfs.GetAddress(0,1),
                            ipv6->GetInterfaceForDevice(staDevices.Get(i)));
    }

    // ---- Rotas entre APs ----
    Ptr<Ipv6> ipv6Ap1 = p2pNodes.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> srAp1 = ipv6RoutingHelper.GetStaticRouting(ipv6Ap1);
    srAp1->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64),
                             ap1ap2Interfaces.GetAddress(1,1),
                             ipv6Ap1->GetInterfaceForDevice(ap1ap2.Get(0)));

    Ptr<Ipv6> ipv6Ap2 = p2pNodes.Get(1)->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> srAp2 = ipv6RoutingHelper.GetStaticRouting(ipv6Ap2);
    srAp2->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64),
                             ap1ap2Interfaces.GetAddress(0,1),
                             ipv6Ap2->GetInterfaceForDevice(ap1ap2.Get(1)));

    Ptr<Ipv6> ipv6Ap3 = p2pNodes.Get(2)->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> srAp3 = ipv6RoutingHelper.GetStaticRouting(ipv6Ap3);
    srAp3->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64),
                             ap1ap3Interfaces.GetAddress(0,1),
                             ipv6Ap3->GetInterfaceForDevice(ap1ap3.Get(1)));
    srAp3->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64),
                             ap2ap3Interfaces.GetAddress(0,1),
                             ipv6Ap3->GetInterfaceForDevice(ap2ap3.Get(1)));

    // ---- Teste com Ping ----
    PingHelper ping(csmaInterfaces1.GetAddress(1,1)); // destino: nó da CSMA1
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(64));
    ping.SetAttribute("Count", UintegerValue(5));
    ApplicationContainer pingApp = ping.Install(wifiStaNodes.Get(0));
    pingApp.Start(Seconds(10.0));
    pingApp.Stop(Seconds(40.0));

    Simulator::Stop(Seconds(50.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

