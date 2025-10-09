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
    uint32_t nWifi = 120;   // ✅ aumentado para 120 nós por rede
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifi", "Number of STA devices in WiFi 1 and 2", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    NodeContainer p2pNodes;
    p2pNodes.Create(3); // n0=AP1, n1=AP2/WiFi3 AP, n2=AP3

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2, ap1ap3, ap2ap3;
    ap1ap2 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(1));
    ap1ap3 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));
    ap2ap3 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));

    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(nWifi);
    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(nWifi);
    NodeContainer wifiStaNodes3;
    wifiStaNodes3.Create(nWifi);

    NodeContainer wifiApNode = p2pNodes.Get(0);
    NodeContainer wifiApNode2 = p2pNodes.Get(2);
    NodeContainer wifiApNode3 = p2pNodes.Get(1);

    // ------------------------------------------------------------
    // ✅ PHY e canais otimizados (reduz interferência e crash)
    // ------------------------------------------------------------
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy1;
    phy1.SetChannel(channel1.Create());
    phy1.Set("ChannelNumber", UintegerValue(1));
    phy1.Set("TxPowerStart", DoubleValue(10));
    phy1.Set("TxPowerEnd", DoubleValue(10));

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy2;
    phy2.SetChannel(channel2.Create());
    phy2.Set("ChannelNumber", UintegerValue(6));
    phy2.Set("TxPowerStart", DoubleValue(10));
    phy2.Set("TxPowerEnd", DoubleValue(10));

    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy3;
    phy3.SetChannel(channel3.Create());
    phy3.Set("ChannelNumber", UintegerValue(11));
    phy3.Set("TxPowerStart", DoubleValue(10));
    phy3.Set("TxPowerEnd", DoubleValue(10));

    WifiMacHelper mac;
    WifiHelper wifi;

    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");
    Ssid ssid3 = Ssid("ns-3-ssid-3");

    NetDeviceContainer staDevices1, staDevices2, staDevices3;
    NetDeviceContainer apDevices1, apDevices2, apDevices3;

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi.Install(phy1, mac, wifiStaNodes1);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices1 = wifi.Install(phy1, mac, wifiApNode);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    apDevices3 = wifi.Install(phy3, mac, wifiApNode3);

    // ------------------------------------------------------------
    // ✅ Aumentar espaçamento entre nós (evita colisão no PHY)
    // ------------------------------------------------------------
    MobilityHelper mobility;
    double spacing = 15.0; // era 5.0

    Ptr<ListPositionAllocator> alloc1 = CreateGridPositionAllocator (nWifi, spacing, 0.0, 0.0);
    Ptr<ListPositionAllocator> alloc2 = CreateGridPositionAllocator (nWifi, spacing, 0.0, 1000.0);
    Ptr<ListPositionAllocator> alloc3 = CreateGridPositionAllocator (nWifi, spacing, 1000.0, 0.0);

    mobility.SetPositionAllocator (alloc1);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes1);

    mobility.SetPositionAllocator (alloc2);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes2);

    mobility.SetPositionAllocator (alloc3);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes3);

    Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator> ();
    apAlloc->Add (Vector ( (std::sqrt(nWifi) * spacing) / 2.0, (std::sqrt(nWifi) * spacing) / 2.0, 0.0 ));
    apAlloc->Add (Vector ( (std::sqrt(nWifi) * spacing) / 2.0, 1000.0 + (std::sqrt(nWifi) * spacing) / 2.0, 0.0 ));
    apAlloc->Add (Vector ( 1000.0 + (std::sqrt(nWifi) * spacing) / 2.0, (std::sqrt(nWifi) * spacing) / 2.0, 0.0 ));
    mobility.SetPositionAllocator (apAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNode);
    mobility.Install (wifiApNode2);
    mobility.Install (wifiApNode3);

    // resto do teu código segue igual (roteamento, IPv6, etc.)
    // ...
}

