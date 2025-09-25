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
    uint32_t nCsma = 200;
    uint32_t nWifi = 200;
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
    cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    // The underlying restriction of 18 is due to the grid position
    // allocator's configuration; the grid layout will exceed the
    // bounding box if more than 18 nodes are provided.
    // if (nWifi > 18)
    // {
    //     std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box"
    //               << std::endl;
    //     return 1;
    // }

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
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
    wifiStaNodes2.Create(16);

    NodeContainer wifiApNode = p2pNodes.Get(0);

    NodeContainer wifiApNode2 = p2pNodes.Get(2);

    // Troque o bloco de canal/phy único por DOIS canais/phys:
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper     phy1;
    phy1.Set("TxPowerStart", DoubleValue(20.0));
    phy1.Set("TxPowerEnd", DoubleValue(20.0));
    phy1.Set("RxGain", DoubleValue(0));
    phy1.Set("CcaEdThreshold", DoubleValue(-62.0));
    phy1.SetChannel(channel1.Create());

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper     phy2;
    phy2.Set("TxPowerStart", DoubleValue(20.0));
    phy2.Set("TxPowerEnd", DoubleValue(20.0));
    phy2.Set("RxGain", DoubleValue(0));
    phy2.Set("CcaEdThreshold", DoubleValue(-62.0));
    phy2.SetChannel(channel2.Create());

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2"); // opcional, mas ajuda

    WifiHelper wifi;

    // --- Primeira rede Wi-Fi (wifiStaNodes <-> wifiApNode) ---
    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy1, mac, wifiStaNodes);

    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices = wifi.Install(phy1, mac, wifiApNode);

    // --- Segunda rede Wi-Fi (wifiStaNodes2 <-> wifiApNode2) ---
    NetDeviceContainer staDevices2;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);

    NetDeviceContainer apDevices2;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    // positionAlloc->Add(Vector(10 + 5, 0.0, 0.0));
    // positionAlloc->Add(Vector(20 + 5, 0.0, 0.0));

    
    for (uint32_t i = 0; i < 200; i++)
    {
        double x = (i % 10) * 10.0;
        double y = (i / 10) * 10.0;
        positionAlloc->Add(Vector(x, y, 0.0));
    }
    
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiStaNodes2);

    mobility.Install(wifiApNode);
    mobility.Install(wifiApNode2);

    InternetStackHelper stack;
    stack.Install(csmaNodes);
    stack.Install(wifiApNode);
    stack.Install(wifiApNode2);
    stack.Install(wifiStaNodes);
    stack.Install(wifiStaNodes2);

    Ipv4AddressHelper address;

    // p2p e CSMA como estavam
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    // *** CORRIGIR AQUI: sub-rede válida para a 1ª rede Wi-Fi ***
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterfaces   = address.Assign(apDevices);

    // 2ª rede Wi-Fi fica em outra sub-rede (já estava ok)
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces2 = address.Assign(staDevices2);
    Ipv4InterfaceContainer apInterfaces2   = address.Assign(apDevices2);

    // p2p restantes como estavam
    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

    address.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);

//   Wifi 10:1:3:0
//                 AP
//  *    *    *    *     ap1ap2
//  |    |    |    |     10:1:1:0
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                  |                ================
//        10:1:5:0  |               |  LAN 2001:2:: - CSMA
//        ap1ap3    |               |  
//                  |               |   10:1:6:0
//                  |               |  ap2ap3
// Wifi2 10:1:4:0   |               |  
//                 AP               | 
//  *    *    *    *                |
//  |    |    |    |                |
// n5   n6   n7   n0 -------------- |

    // PingHelper ping(wifiInterfaces2.GetAddress(0));
    // ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    // ping.SetAttribute("Size", UintegerValue(512));
    // ping.SetAttribute("Count", UintegerValue(10));

    // // Ptr<Node> apNode = ap1.Get(0);
    // ApplicationContainer pingApp = ping.Install(csmaNodes.Get(0));
    // pingApp.Start(Seconds(30.0));
    // pingApp.Stop(Seconds(110.0));

    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(csmaNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(50.0));

    UdpEchoClientHelper echoClient(csmaInterfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(50.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(120.0));

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
