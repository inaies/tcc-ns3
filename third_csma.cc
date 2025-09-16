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
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
    // cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    // if (nWifi > 18)
    // {
    //     std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box"
    //               << std::endl;
    //     return 1;
    // }

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


    Ipv6StaticRoutingHelper ipv6RoutingHelper;

    // *** PILHA IPv6 ***
    InternetStackHelper stack;
    stack.SetRoutingHelper(ipv6RoutingHelper);
    stack.Install(csmaNodes);
    stack.Install(csmaNodes2);
    stack.Install(csmaNodes3);


    // *** ENDEREÇAMENTO IPv6 ***
    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);
    ap1ap2Interfaces.SetForwarding(0, true);
    ap1ap2Interfaces.SetForwarding(1, true);
    ap1ap2Interfaces.SetDefaultRouteInAllNodes(0);
    ap1ap2Interfaces.SetDefaultRouteInAllNodes(1);

    std::cout << "ap1ap2Interfaces:" << std::endl;
    for (uint32_t i = 0; i < ap1ap2Interfaces.GetN(); i++){
        for (uint32_t j = 0; j < 2; j++){
            if (ap1ap2Interfaces.GetAddress(i, j).IsInitialized()){
                std::cout << " Node " << i
                          << " addr[" << j << "] ="
                          << ap1ap2Interfaces.GetAddress(i, j) << std::endl;
            }
        }
    }


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
    ap1ap3Interfaces.SetDefaultRouteInAllNodes(1);
    ap1ap3Interfaces.SetDefaultRouteInAllNodes(1);


    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);
    ap2ap3Interfaces.SetForwarding(0, true);
    ap2ap3Interfaces.SetForwarding(1, true);
    ap2ap3Interfaces.SetDefaultRouteInAllNodes(1);
    ap2ap3Interfaces.SetDefaultRouteInAllNodes(1);


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

    Ptr<Ipv6> ipv6Ap1 = p2pNodes.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> srAp1 = ipv6RoutingHelper.GetStaticRouting(ipv6Ap1);

    srAp1->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64),
                            ap1ap2Interfaces.GetAddress(1,1), ipv6Ap1->GetInterfaceForDevice(ap1ap2.Get(0)));

    srAp1->AddNetworkRouteTo(Ipv6Address("2001:4::"), Ipv6Prefix(64),
                            ap1ap3Interfaces.GetAddress(1,1), ipv6Ap1->GetInterfaceForDevice(ap1ap3.Get(0)));

    Ptr<Ipv6> ipv6Ap2 = p2pNodes.Get(1)->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> srAp2 = ipv6RoutingHelper.GetStaticRouting(ipv6Ap2);

    srAp2->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64),
                            ap1ap2Interfaces.GetAddress(0,1), ipv6Ap2->GetInterfaceForDevice(ap1ap2.Get(1)));
    srAp2->AddNetworkRouteTo(Ipv6Address("2001:4::"), Ipv6Prefix(64),
                            ap2ap3Interfaces.GetAddress(1,1), ipv6Ap2->GetInterfaceForDevice(ap2ap3.Get(0)));

    Ptr<Ipv6> ipv6Ap3 = p2pNodes.Get(2)->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> srAp3 = ipv6RoutingHelper.GetStaticRouting(ipv6Ap3);

    srAp3->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64),
                            ap1ap3Interfaces.GetAddress(0,1), ipv6Ap3->GetInterfaceForDevice(ap1ap3.Get(1)));
    srAp3->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64),
                            ap2ap3Interfaces.GetAddress(0,1), ipv6Ap3->GetInterfaceForDevice(ap2ap3.Get(1)));

    // *** PING IPv6 ***
    PingHelper ping(csmaInterfaces2.GetAddress(0, 1)); // endereço global do nó
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(512));
    ping.SetAttribute("Count", UintegerValue(10));

    ApplicationContainer pingApp = ping.Install(csmaNodes3.Get(0));
    pingApp.Start(Seconds(30.0));
    pingApp.Stop(Seconds(110.0));


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

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

