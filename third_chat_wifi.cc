// ... (mesmos includes)
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ping-helper.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeWifiNetworks");

int
main(int argc, char* argv[])
{
    LogComponentEnable("Ping", LOG_LEVEL_INFO);

    bool verbose = true;
    uint32_t nWifi1 = 200;
    uint32_t nWifi2 = 16;
    uint32_t nWifi3 = 50;   // terceira rede wifi
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifi1", "Number of wifi STA devices in net1", nWifi1);
    cmd.AddValue("nWifi2", "Number of wifi STA devices in net2", nWifi2);
    cmd.AddValue("nWifi3", "Number of wifi STA devices in net3", nWifi3);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // backbone: 3 nós APs interconectados por P2P
    NodeContainer p2pNodes;
    p2pNodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2, ap1ap3, ap2ap3;
    ap1ap2 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(1));
    ap1ap3 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));
    ap2ap3 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));

    // STA nodes
    NodeContainer wifiStaNodes1, wifiStaNodes2, wifiStaNodes3;
    wifiStaNodes1.Create(nWifi1);
    wifiStaNodes2.Create(nWifi2);
    wifiStaNodes3.Create(nWifi3);

    // AP nodes (já criados no backbone)
    NodeContainer wifiApNode1 = p2pNodes.Get(0);
    NodeContainer wifiApNode2 = p2pNodes.Get(2);
    NodeContainer wifiApNode3 = p2pNodes.Get(1); // o nó central agora é AP da terceira rede

    // três canais/phys
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper     phy1;
    phy1.SetChannel(channel1.Create());

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper     phy2;
    phy2.SetChannel(channel2.Create());

    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper     phy3;
    phy3.SetChannel(channel3.Create());

    WifiHelper wifi;
    WifiMacHelper mac;

    // SSIDs diferentes
    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");
    Ssid ssid3 = Ssid("ns-3-ssid-3");

    // Rede WiFi1
    NetDeviceContainer staDevices1, apDevices1;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi.Install(phy1, mac, wifiStaNodes1);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices1 = wifi.Install(phy1, mac, wifiApNode1);

    // Rede WiFi2
    NetDeviceContainer staDevices2, apDevices2;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    // Rede WiFi3
    NetDeviceContainer staDevices3, apDevices3;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    apDevices3 = wifi.Install(phy3, mac, wifiApNode3);

    // Mobilidade
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    // exemplo: distribuir 200 nós em grid
    for (uint32_t i = 0; i < nWifi1; i++)
    {
        double x = (i % 20) * 5.0;
        double y = (i / 20) * 5.0;
        positionAlloc->Add(Vector(x, y, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes1);
    mobility.Install(wifiStaNodes2);
    mobility.Install(wifiStaNodes3);
    mobility.Install(wifiApNode1);
    mobility.Install(wifiApNode2);
    mobility.Install(wifiApNode3);

    // Pilha IP
    InternetStackHelper stack;
    stack.Install(p2pNodes);
    stack.Install(wifiStaNodes1);
    stack.Install(wifiStaNodes2);
    stack.Install(wifiStaNodes3);

    Ipv4AddressHelper address;

    // endereços backbone P2P
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);

    // redes WiFi
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces1 = address.Assign(staDevices1);
    Ipv4InterfaceContainer apInterfaces1   = address.Assign(apDevices1);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces2 = address.Assign(staDevices2);
    Ipv4InterfaceContainer apInterfaces2   = address.Assign(apDevices2);

    address.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces3 = address.Assign(staDevices3);
    Ipv4InterfaceContainer apInterfaces3   = address.Assign(apDevices3);

    // Aplicações
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes2.Get(0)); // servidor em STA da rede2
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(50.0));

    UdpEchoClientHelper echoClient(wifiInterfaces2.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes1.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(50.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(60.0));

    if (tracing)
    {
        phy1.EnablePcap("wifi1", apDevices1.Get(0));
        phy2.EnablePcap("wifi2", apDevices2.Get(0));
        phy3.EnablePcap("wifi3", apDevices3.Get(0));
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

