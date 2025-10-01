// third_three_wifi_fixed.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

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
main (int argc, char *argv[])
{
  bool tracing = true;
  uint32_t nWifi = 150; // per network (default). Change via --nWifi
  uint32_t nAp = 3;    // number of APs / networks

  LogComponentEnable("Ping", LOG_LEVEL_INFO);

  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.AddValue ("nWifi", "Number of WiFi STA nodes per network", nWifi);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.Parse (argc, argv);

  // Basic logging for Ping / UdpEcho if you want:
  // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create 3 nodes that will act as wired routers/AP hosts connected P2P
  NodeContainer p2pNodes;
  p2pNodes.Create (3);

  // Create point-to-point links between the three backbone nodes (triangle)
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d01 = p2p.Install (p2pNodes.Get(0), p2pNodes.Get(1));
  NetDeviceContainer d02 = p2p.Install (p2pNodes.Get(0), p2pNodes.Get(2));
  NetDeviceContainer d12 = p2p.Install (p2pNodes.Get(1), p2pNodes.Get(2));

  // For each Wi-Fi network: create STA nodes and a single AP node (AP is one of the p2p nodes)
  NodeContainer staNet1, staNet2, staNet3;
  staNet1.Create (nWifi);
  staNet2.Create (nWifi);
  staNet3.Create (nWifi);

  // AP nodes are p2pNodes: choose 0,2,1 respectively so triangle linking remains
  NodeContainer apNode1 (p2pNodes.Get (0));
  NodeContainer apNode2 (p2pNodes.Get (2));
  NodeContainer apNode3 (p2pNodes.Get (1));

  // Create independent channels and phys for each Wi-Fi network
  YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy1;
  phy1.SetChannel (channel1.Create ());

  YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy2;
  phy2.SetChannel (channel2.Create ());

  YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy3;
  phy3.SetChannel (channel3.Create ());

  // Wifi helpers & MAC config (reuse WifiHelper but different phys)
  WifiHelper wifi;

  WifiMacHelper mac;
  Ssid ssid1 = Ssid ("net-ssid-1");
  Ssid ssid2 = Ssid ("net-ssid-2");
  Ssid ssid3 = Ssid ("net-ssid-3");

  // Create STA and AP devices per network (each uses its own phy)
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDev1 = wifi.Install (phy1, mac, staNet1);

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid1));
  NetDeviceContainer apDev1 = wifi.Install (phy1, mac, apNode1);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid2),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDev2 = wifi.Install (phy2, mac, staNet2);

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid2));
  NetDeviceContainer apDev2 = wifi.Install (phy2, mac, apNode2);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid3),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDev3 = wifi.Install (phy3, mac, staNet3);

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid3));
  NetDeviceContainer apDev3 = wifi.Install (phy3, mac, apNode3);

  // Mobility: create separate position allocators so networks don't overlap
  MobilityHelper mobility;

  // spacing & offsets chosen so each WiFi network occupies a distinct area
  double spacing = 5.0;
  // compute grid allocator for each STA network
  Ptr<ListPositionAllocator> alloc1 = CreateGridPositionAllocator (nWifi, spacing, 0.0, 0.0);
  Ptr<ListPositionAllocator> alloc2 = CreateGridPositionAllocator (nWifi, spacing, 0.0, 1000.0); // offset Y far away
  Ptr<ListPositionAllocator> alloc3 = CreateGridPositionAllocator (nWifi, spacing, 1000.0, 0.0); // offset X far away

  // set positions for STA nodes
  mobility.SetPositionAllocator (alloc1);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNet1);

  mobility.SetPositionAllocator (alloc2);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNet2);

  mobility.SetPositionAllocator (alloc3);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNet3);

  // place APs near centers of their corresponding STA grids
  Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator> ();
  // AP1 center (approx)
  apAlloc->Add (Vector ( (std::sqrt(nWifi) * spacing) / 2.0, (std::sqrt(nWifi) * spacing) / 2.0, 0.0 )); // near origin
  // AP2 center (offset Y)
  apAlloc->Add (Vector ( (std::sqrt(nWifi) * spacing) / 2.0, 1000.0 + (std::sqrt(nWifi) * spacing) / 2.0, 0.0 ));
  // AP3 center (offset X)
  apAlloc->Add (Vector ( 1000.0 + (std::sqrt(nWifi) * spacing) / 2.0, (std::sqrt(nWifi) * spacing) / 2.0, 0.0 ));

  mobility.SetPositionAllocator (apAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode1);
  mobility.Install (apNode2);
  mobility.Install (apNode3);

  // Internet stack and addressing
  InternetStackHelper stack;
  stack.Install (p2pNodes);
  stack.Install (staNet1);
  stack.Install (staNet2);
  stack.Install (staNet3);

  Ipv4AddressHelper address;

  // p2p backbone subnets
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = address.Assign (d01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i02 = address.Assign (d02);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = address.Assign (d12);

  // Wi-Fi net 1
  address.SetBase ("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifSta1 = address.Assign (staDev1);
  Ipv4InterfaceContainer ifAp1  = address.Assign (apDev1);

  // Wi-Fi net 2
  address.SetBase ("10.2.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifSta2 = address.Assign (staDev2);
  Ipv4InterfaceContainer ifAp2  = address.Assign (apDev2);

  // Wi-Fi net 3
  address.SetBase ("10.2.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ifSta3 = address.Assign (staDev3);
  Ipv4InterfaceContainer ifAp3  = address.Assign (apDev3);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install a very light server/client to test connectivity (one per network)
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (staNet3.Get (0)); // server on net3 first STA
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (50.0));

  UdpEchoClientHelper echoClient (ifSta3.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (10.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (64));

  // ping some STAs from net1 and net2 to the server on net3
  ApplicationContainer clientApps1 = echoClient.Install (staNet1.Get (0));
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (50.0));

  // ApplicationContainer clientApps2 = echoClient.Install (staNet2.Get (0));
  // clientApps2.Start (Seconds (2.5));
  // clientApps2.Stop (Seconds (50.0));

  // optional pcap tracing for debugging (be careful: many STAs => big pcap)
  if (tracing)
    {
      phy1.EnablePcap ("wifi-net1-ap", apDev1.Get (0));
      phy2.EnablePcap ("wifi-net2-ap", apDev2.Get (0));
      phy3.EnablePcap ("wifi-net3-ap", apDev3.Get (0));
      p2p.EnablePcapAll ("p2p-backbone");
    }

  Simulator::Stop (Seconds (60.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

