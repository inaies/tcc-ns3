// ... (mesmos includes)
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

int
main(int argc, char* argv[])
{
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
    p2pNodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2, ap1ap3, ap2ap3;
    ap1ap2 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(1));
    ap1ap3 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));
    ap2ap3 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));
    
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes.Get(0));
    csmaNodes.Create(nCsma);

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    NodeContainer csmaNodes2;
    csmaNodes2.Add(p2pNodes.Get(1));
    csmaNodes2.Create(nCsma);

    NetDeviceContainer csmaDevices2;
    csmaDevices2 = csma.Install(csmaNodes2);

    NodeContainer csmaNodes3;
    csmaNodes3.Add(p2pNodes.Get(2));
    csmaNodes3.Create(nCsma);

    NetDeviceContainer csmaDevices3;
    csmaDevices3 = csma.Install(csmaNodes3);

    // NodeContainer wifiStaNodes;
    // wifiStaNodes.Create(nWifi);

    // NodeContainer wifiStaNodes2;
    // wifiStaNodes2.Create(nWifi);

    // NodeContainer wifiApNode = p2pNodes.Get(0);
    // NodeContainer wifiApNode2 = p2pNodes.Get(2);

    // Dois canais Wi-Fi separados
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy1;
    phy1.SetChannel(channel1.Create());

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy2;
    phy2.SetChannel(channel2.Create());

    // WifiMacHelper mac;
    // WifiHelper wifi;

    // Ssid ssid1 = Ssid("ns-3-ssid-1");
    // Ssid ssid2 = Ssid("ns-3-ssid-2");

    // // --- Primeira rede Wi-Fi ---
    // NetDeviceContainer staDevices;
    // mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    // staDevices = wifi.Install(phy1, mac, wifiStaNodes);

    // NetDeviceContainer apDevices;
    // mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    // apDevices = wifi.Install(phy1, mac, wifiApNode);

    // // --- Segunda rede Wi-Fi ---
    // NetDeviceContainer staDevices2;
    // mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    // staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);

    // NetDeviceContainer apDevices2;
    // mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    // apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    // Mobilidade
    // MobilityHelper mobility;
    // mobility.SetPositionAllocator("ns3::GridPositionAllocator",
    //                               "MinX", DoubleValue(0.0),
    //                               "MinY", DoubleValue(0.0),
    //                               "DeltaX", DoubleValue(5.0),
    //                               "DeltaY", DoubleValue(10.0),
    //                               "GridWidth", UintegerValue(3),
    //                               "LayoutType", StringValue("RowFirst"));

    // mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
    //                           "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    // mobility.Install(wifiStaNodes);
    // mobility.Install(wifiStaNodes2);

    // mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    // mobility.Install(wifiApNode);
    // mobility.Install(wifiApNode2);


    // RipNgHelper ripNg;
    // Ipv6ListRoutingHelper listRh;
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    // listRh.Add(ripNg, 0);
    // listRh.Add(ipv6RoutingHelper, 5);

    // InternetStackHelper stack;
    // *** PILHA IPv6 ***
    InternetStackHelper stack;
    // stack.SetRoutingHelper(listRh);    
    stack.SetRoutingHelper(ipv6RoutingHelper);
    stack.Install(csmaNodes);
    stack.Install(csmaNodes2);
    stack.Install(csmaNodes3);
    // stack.Install(wifiApNode);
    // stack.Install(wifiApNode2);
    // stack.Install(wifiStaNodes);
    // stack.Install(wifiStaNodes2);

    // *** ENDEREÇAMENTO IPv6 ***
    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);
    ap1ap2Interfaces.SetForwarding(0, true);
    ap1ap2Interfaces.SetForwarding(1, true);

    address.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);
    csmaInterfaces.SetForwarding(0, true);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer csmaInterfaces2 = address.Assign(csmaDevices2);
    csmaInterfaces2.SetForwarding(0, true);

    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer csmaInterfaces3 = address.Assign(csmaDevices3);
    csmaInterfaces3.SetForwarding(0, true);

    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);
    ap1ap3Interfaces.SetForwarding(0, true);
    ap1ap3Interfaces.SetForwarding(1, true);

    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);
    ap2ap3Interfaces.SetForwarding(0, true);
    ap2ap3Interfaces.SetForwarding(1, true);


    for (uint32_t i = 1; i < csmaNodes.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = csmaNodes.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(csmaInterfaces.GetAddress(0,1), 1);
    }

    for (uint32_t i = 1; i < csmaNodes2.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = csmaNodes2.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(csmaInterfaces2.GetAddress(0,1), 1);
    }

    for (uint32_t i = 1; i < csmaNodes3.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = csmaNodes3.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(csmaInterfaces3.GetAddress(0,1), 1);
    }


    // for (uint32_t i = 0; i < wifiStaNodes.GetN(); i++)
    // {
    //     Ptr<Ipv6> ipv6 = wifiStaNodes.Get(i)->GetObject<Ipv6>();
    //     Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
    //     uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices.Get(i));
    //     sr->SetDefaultRoute(apInterfaces.GetAddress(0,1), ifSta);
    // }

    // for (uint32_t i = 0; i < wifiStaNodes2.GetN(); i++)
    // {
    //     Ptr<Ipv6> ipv6 = wifiStaNodes2.Get(i)->GetObject<Ipv6>();
    //     Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
    //     uint32_t ifSta2 = ipv6->GetInterfaceForDevice(staDevices2.Get(i));
    //     sr->SetDefaultRoute(apInterfaces2.GetAddress(0,1), ifSta2);
    // }

    // AP1 →rota para rede CSMA (2001:2::/64)
    // Ptr<Ipv6> ipv6Ap1 = wifiApNode.Get(0)->GetObject<Ipv6>();
    // uint32_t ifAp1ToAp2 = ipv6Ap1->GetInterfaceForDevice(ap1ap2.Get(0));
    // Ptr<Ipv6StaticRouting> srAp1 = ipv6RoutingHelper.GetStaticRouting(ipv6Ap1);
    // // srAp1->SetDefaultRoute(ap1ap2Interfaces.GetAddress(1,1), ifAp1ToAp2);
    // srAp1->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64),
    //                             ap1ap2Interfaces.GetAddress(1,1), 1);

    // // AP2 → rota de volta para a rede do STA via AP1
    // Ptr<Ipv6> ipv6Ap2 = p2pNodes.Get(1)->GetObject<Ipv6>();
    // uint32_t ifAp2ToAp1 = ipv6Ap1->GetInterfaceForDevice(ap1ap2.Get(1));
    // Ptr<Ipv6StaticRouting> srAp2 = ipv6RoutingHelper.GetStaticRouting(ipv6Ap2);
    // srAp2->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64),
    //                             ap1ap2Interfaces.GetAddress(0,1), 1);

    // *** PING IPv6 ***
    PingHelper ping(ap1ap2Interfaces.GetAddress(0, 1)); // endereço global do nó
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(512));
    ping.SetAttribute("Count", UintegerValue(10));

    ApplicationContainer pingApp = ping.Install(csmaNodes.Get(0));
    pingApp.Start(Seconds(30.0));
    pingApp.Stop(Seconds(110.0));


    // for (uint32_t i = 0; i < NodeList::GetNNodes(); i++)
    // {
    //     Ptr<Node> node = NodeList::GetNode(i);
    //     Ptr<OutputStreamWrapper> routingStream = 
    //         Create<OutputStreamWrapper> ("routing-" + std::to_string(i) + ".txt", std::ios::out);
        
    //     ipv6RoutingHelper.PrintRoutingTableAt (Seconds (100.0), node, routingStream);
    // }


//   Wifi 2001:3::
//                 AP
//  *    *    *    *     ap1ap2
//  |    |    |    |    2001:1::
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                  |                ================
//        2001:5::  |               |  LAN 2001:2:: - CSMA
//        ap1ap3    |               |  
//                  |               |  2001:6::
//                  |               |  ap2ap3
// Wifi2 2001:4::   |               |  
//                 AP               | 
//  *    *    *    *                |
//  |    |    |    |                |
// n5   n6   n7   n0 -------------- |

    // Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(120.0));

    if (tracing)
    {
        phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        pointToPoint.EnablePcapAll("third");
        phy1.EnablePcap("third", csmaDevices2.Get(0));
        csma.EnablePcap("third", csmaDevices.Get(0), true);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

