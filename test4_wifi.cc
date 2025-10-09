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

static Ptr<ListPositionAllocator>
CreateGridPositionAllocator (uint32_t nNodes, double spacing, double offsetX, double offsetY)
{
  Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator> ();
  uint32_t cols = std::ceil(std::sqrt(static_cast<double>(nNodes)));
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
    uint32_t nWifi = 120; // agora com 120 dispositivos por rede
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifi", "Number of STA devices in WiFi 1, 2 and 3", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    NodeContainer p2pNodes;
    p2pNodes.Create(3); // n0=AP1, n1=AP2, n2=AP3

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(1));
    NetDeviceContainer ap1ap3 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));
    NetDeviceContainer ap2ap3 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));

    // --- Criação das redes Wi-Fi ---
    NodeContainer wifiStaNodes1; wifiStaNodes1.Create(nWifi);
    NodeContainer wifiStaNodes2; wifiStaNodes2.Create(nWifi);
    NodeContainer wifiStaNodes3; wifiStaNodes3.Create(nWifi);

    NodeContainer wifiApNode = p2pNodes.Get(0);
    NodeContainer wifiApNode2 = p2pNodes.Get(2);
    NodeContainer wifiApNode3 = p2pNodes.Get(1);

    // --- Configuração PHY/MAC ---
    YansWifiChannelHelper channel1, channel2, channel3;
    channel1.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                "Exponent", DoubleValue(2.5),
                                "ReferenceLoss", DoubleValue(40.0));
    channel2.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                "Exponent", DoubleValue(2.5),
                                "ReferenceLoss", DoubleValue(40.0));
    channel3.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                "Exponent", DoubleValue(2.5),
                                "ReferenceLoss", DoubleValue(40.0));
    channel1.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel2.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel3.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    YansWifiPhyHelper phy1, phy2, phy3;
    phy1.SetChannel(channel1.Create());
    phy2.SetChannel(channel2.Create());
    phy3.SetChannel(channel3.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");
    Ssid ssid3 = Ssid("ns-3-ssid-3");

    NetDeviceContainer staDevices1, apDevices1;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi.Install(phy1, mac, wifiStaNodes1);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices1 = wifi.Install(phy1, mac, wifiApNode);

    NetDeviceContainer staDevices2, apDevices2;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    NetDeviceContainer staDevices3, apDevices3;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    apDevices3 = wifi.Install(phy3, mac, wifiApNode3);

    // --- Mobilidade ---
    MobilityHelper mobility;
    double spacing = 15.0; // espaçamento maior para 120 nós
    double offset = 1500.0; // distância entre redes

    Ptr<ListPositionAllocator> alloc1 = CreateGridPositionAllocator(nWifi, spacing, 0.0, 0.0);
    Ptr<ListPositionAllocator> alloc2 = CreateGridPositionAllocator(nWifi, spacing, 0.0, offset);
    Ptr<ListPositionAllocator> alloc3 = CreateGridPositionAllocator(nWifi, spacing, offset, 0.0);

    mobility.SetPositionAllocator(alloc1);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes1);

    mobility.SetPositionAllocator(alloc2);
    mobility.Install(wifiStaNodes2);

    mobility.SetPositionAllocator(alloc3);
    mobility.Install(wifiStaNodes3);

    // Colocar APs próximos ao centro de suas redes
    Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator>();
    apAlloc->Add(Vector((std::sqrt(nWifi) * spacing) / 2.0, (std::sqrt(nWifi) * spacing) / 2.0, 0.0));          // AP1
    apAlloc->Add(Vector((std::sqrt(nWifi) * spacing) / 2.0, offset + (std::sqrt(nWifi) * spacing) / 2.0, 0.0)); // AP2
    apAlloc->Add(Vector(offset + (std::sqrt(nWifi) * spacing) / 2.0, (std::sqrt(nWifi) * spacing) / 2.0, 0.0)); // AP3

    mobility.SetPositionAllocator(apAlloc);
    mobility.Install(p2pNodes);

    // --- Pilhas de Roteamento ---
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

    // --- Forwarding nos roteadores ---
    for (uint32_t i = 0; i < p2pNodes.GetN(); ++i)
    {
        Ptr<Ipv6> ipv6 = p2pNodes.Get(i)->GetObject<Ipv6>();
        ipv6->SetForwarding(0, true);
    }

    // --- Rotas Estáticas ---
    Ipv6Address ap1Addr = apInterfaces1.GetAddress(0, 1);
    Ipv6Address ap2Addr = apInterfaces3.GetAddress(0, 1);
    Ipv6Address ap3Addr = apInterfaces2.GetAddress(0, 1);

    for (uint32_t i = 0; i < nWifi; i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes1.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(ap1Addr, ipv6->GetInterfaceForDevice(staDevices1.Get(i)));
    }
    for (uint32_t i = 0; i < nWifi; i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes2.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(ap3Addr, ipv6->GetInterfaceForDevice(staDevices2.Get(i)));
    }
    for (uint32_t i = 0; i < nWifi; i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes3.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(ap2Addr, ipv6->GetInterfaceForDevice(staDevices3.Get(i)));
    }

    // --- Aplicações de teste ---
    UdpEchoServerHelper echoServer (9);
    ApplicationContainer serverApps = echoServer.Install (wifiStaNodes3.Get(0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (50.0));

    UdpEchoClientHelper echoClient (wifiInterfaces2.GetAddress(0, 1), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (2));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (64));

    ApplicationContainer clientApps1 = echoClient.Install (wifiStaNodes1.Get(5));
    clientApps1.Start (Seconds (2.0));
    clientApps1.Stop (Seconds (50.0));

    Simulator::Stop(Seconds(120.0));

    if (tracing)
    {
        pointToPoint.EnablePcapAll("third");
        phy1.EnablePcap("third", apDevices1.Get(0));
        phy3.EnablePcap("third", apDevices3.Get(0));
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

