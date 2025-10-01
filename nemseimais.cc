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
    uint32_t nWifi = 100; // ajuste para 200 se quiser 200 nós por rede
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
    cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

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

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);

    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(nWifi);

    NodeContainer wifiStaNodes3;
    wifiStaNodes3.Create(nWifi);

    NodeContainer wifiApNode = NodeContainer(p2pNodes.Get(0));
    NodeContainer wifiApNode2 = NodeContainer(p2pNodes.Get(2));
    NodeContainer wifiApNode3 = NodeContainer(p2pNodes.Get(1));

    // Dois canais/phys (na verdade três aqui)
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

    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper     phy3;
    phy3.Set("TxPowerStart", DoubleValue(20.0));
    phy3.Set("TxPowerEnd", DoubleValue(20.0));
    phy3.Set("RxGain", DoubleValue(0));
    phy3.Set("CcaEdThreshold", DoubleValue(-62.0));
    phy3.SetChannel(channel3.Create());    

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");
    Ssid ssid3 = Ssid("ns-3-ssid-3");

    WifiHelper wifi;

    // --- Primeira rede Wi-Fi ---
    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy1, mac, wifiStaNodes);

    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices = wifi.Install(phy1, mac, wifiApNode);

    // --- Segunda rede Wi-Fi ---
    NetDeviceContainer staDevices2;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);

    NetDeviceContainer apDevices2;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    // --- Terceira rede Wi-Fi ---
    NetDeviceContainer staDevices3;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);

    NetDeviceContainer apDevices3;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    apDevices3 = wifi.Install(phy3, mac, wifiApNode3);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    // Gera posições suficientes para todas as STAs (nWifi por rede × 3 redes)
    uint32_t totalSta = nWifi * 3;
    for (uint32_t i = 0; i < totalSta; i++)
    {
        double x = (i % 10) * 10.0;
        double y = (i / 10) * 10.0;
        positionAlloc->Add(Vector(x, y, 0.0));
    }

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiStaNodes2);
    mobility.Install(wifiStaNodes3);

    // Posiciona APs (p2p nodes)
    MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
    apPos->Add(Vector(0.0, 0.0, 0.0));   // AP1 (p2pNodes.Get(0))
    apPos->Add(Vector(50.0, 0.0, 0.0));  // AP3 (p2pNodes.Get(1))
    apPos->Add(Vector(100.0, 0.0, 0.0)); // AP2 (p2pNodes.Get(2))
    mobilityAp.SetPositionAllocator(apPos);
    mobilityAp.Install(wifiApNode);
    mobilityAp.Install(wifiApNode3);
    mobilityAp.Install(wifiApNode2);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiApNode2);
    stack.Install(wifiApNode3);
    stack.Install(wifiStaNodes);
    stack.Install(wifiStaNodes2);
    stack.Install(wifiStaNodes3);

    Ipv4AddressHelper address;

    // p2p e CSMA como estavam
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    // *** TERCEIRA rede Wi-Fi: atribui IPs tanto para STA quanto para AP ***
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces3 = address.Assign(staDevices3);
    Ipv4InterfaceContainer apInterfaces3   = address.Assign(apDevices3); // <--- correção importante

    // Primeira rede Wi-Fi
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterfaces   = address.Assign(apDevices);

    // 2ª rede Wi-Fi
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces2 = address.Assign(staDevices2);
    Ipv4InterfaceContainer apInterfaces2   = address.Assign(apDevices2);

    // p2p restantes
    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

    address.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);

    // Servidor na terceira rede (uma STA)
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes3.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(50.0));

    // Cliente na primeira rede (outra STA) apontando para o IP do servidor (wifiInterfaces3)
    UdpEchoClientHelper echoClient(wifiInterfaces3.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(50.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(120.0));

    // if (tracing)
    // {
    //     phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    //     pointToPoint.EnablePcapAll("third");
    //     phy1.EnablePcap("third", apDevices.Get(0));
    //     phy3.EnablePcap("third", staDevices3.Get(0), true);
    // }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

