// corrected_three_wifi_large.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeWifiLarge");

int main (int argc, char *argv[])
{
  bool tracing = false;
  uint32_t nWifi = 200; // por rede
  CommandLine cmd;
  cmd.AddValue ("nWifi", "Number of wifi STA devices per network", nWifi);
  cmd.AddValue ("tracing", "Enable pcap traces", tracing);
  cmd.Parse (argc, argv);

  // nós AP (usar p2pNodes como nós que ligam as redes)
  NodeContainer p2pNodes;
  p2pNodes.Create (3);

  // ligações ponto-a-ponto entre APs
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer p2p01 = p2p.Install (p2pNodes.Get (0), p2pNodes.Get (1));
  NetDeviceContainer p2p02 = p2p.Install (p2pNodes.Get (0), p2pNodes.Get (2));
  NetDeviceContainer p2p12 = p2p.Install (p2pNodes.Get (1), p2pNodes.Get (2));

  // Cria STA nodes para cada rede
  NodeContainer staNet1, staNet2, staNet3;
  staNet1.Create (nWifi);
  staNet2.Create (nWifi);
  staNet3.Create (nWifi);

  // AP nodes (cada AP é um dos nós p2pNodes)
  NodeContainer ap1 (p2pNodes.Get (0));
  NodeContainer ap2 (p2pNodes.Get (1));
  NodeContainer ap3 (p2pNodes.Get (2));

  // PHY / CHAN separados (evita interferência direta)
  YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy1 = YansWifiPhyHelper::Default ();
  phy1.SetChannel (channel1.Create ());
  phy1.Set ("TxPowerStart", DoubleValue (18.0));
  phy1.Set ("TxPowerEnd", DoubleValue (18.0));

  YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy2 = YansWifiPhyHelper::Default ();
  phy2.SetChannel (channel2.Create ());
  phy2.Set ("TxPowerStart", DoubleValue (18.0));
  phy2.Set ("TxPowerEnd", DoubleValue (18.0));

  YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy3 = YansWifiPhyHelper::Default ();
  phy3.SetChannel (channel3.Create ());
  phy3.Set ("TxPowerStart", DoubleValue (18.0));
  phy3.Set ("TxPowerEnd", DoubleValue (18.0));

  WifiHelper wifi;
  // fixa uma taxa constante para testes (evita comportamento adaptativo)
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate6Mbps"),
                                "ControlMode", StringValue ("OfdmRate6Mbps"));

  WifiMacHelper mac;
  Ssid ssid1 = Ssid ("network-1");
  Ssid ssid2 = Ssid ("network-2");
  Ssid ssid3 = Ssid ("network-3");

  // Instalação: rede1
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid1), "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDev1 = wifi.Install (phy1, mac, staNet1);
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid1));
  NetDeviceContainer apDev1 = wifi.Install (phy1, mac, ap1);

  // rede2
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid2), "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDev2 = wifi.Install (phy2, mac, staNet2);
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid2));
  NetDeviceContainer apDev2 = wifi.Install (phy2, mac, ap2);

  // rede3
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid3), "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDev3 = wifi.Install (phy3, mac, staNet3);
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid3));
  NetDeviceContainer apDev3 = wifi.Install (phy3, mac, ap3);

  // MOBILIDADE: posições separadas (3 áreas distintas). APs no centro de cada área.
  MobilityHelper mobility;

  // rede1 positions (grid 20x10 = 200, espaçamento 5m)
  Ptr<ListPositionAllocator> pos1 = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < nWifi; ++i)
    {
      double x = (i % 20) * 5.0;
      double y = (i / 20) * 5.0;
      pos1->Add (Vector (x, y, 0.0));
    }
  mobility.SetPositionAllocator (pos1);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNet1);

  // rede2 positions deslocadas em X (evita overlap)
  Ptr<ListPositionAllocator> pos2 = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < nWifi; ++i)
    {
      double x = 200.0 + (i % 20) * 5.0;
      double y = (i / 20) * 5.0;
      pos2->Add (Vector (x, y, 0.0));
    }
  mobility.SetPositionAllocator (pos2);
  mobility.Install (staNet2);

  // rede3 positions deslocadas em Y
  Ptr<ListPositionAllocator> pos3 = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < nWifi; ++i)
    {
      double x = (i % 20) * 5.0;
      double y = 200.0 + (i / 20) * 5.0;
      pos3->Add (Vector (x, y, 0.0));
    }
  mobility.SetPositionAllocator (pos3);
  mobility.Install (staNet3);

  // APs: coloque-os próximos ao centro das suas áreas
  Ptr<ListPositionAllocator> posAp = CreateObject<ListPositionAllocator> ();
  posAp->Add (Vector (45.0, 22.5, 0.0));   // AP1 central rede1
  posAp->Add (Vector (245.0, 22.5, 0.0));  // AP2 central rede2
  posAp->Add (Vector (45.0, 222.5, 0.0));  // AP3 central rede3
  mobility.SetPositionAllocator (posAp);
  mobility.Install (ap1);
  mobility.Install (ap2);
  mobility.Install (ap3);

  // PILHA INTERNET (IPv4)
  InternetStackHelper stack;
  stack.Install (p2pNodes); // APs também recebem pilha
  stack.Install (staNet1);
  stack.Install (staNet2);
  stack.Install (staNet3);

  // ENDEREÇAMENTO IPv4
  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2p01If = address.Assign (p2p01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2p02If = address.Assign (p2p02);

  address.SetBase ("10.1.3.0", "255.255.255.0"); // wifi rede1
  Ipv4InterfaceContainer sta1If = address.Assign (staDev1);
  Ipv4InterfaceContainer ap1If  = address.Assign (apDev1);

  address.SetBase ("10.1.4.0", "255.255.255.0"); // wifi rede2
  Ipv4InterfaceContainer sta2If = address.Assign (staDev2);
  Ipv4InterfaceContainer ap2If  = address.Assign (apDev2);

  address.SetBase ("10.1.5.0", "255.255.255.0"); // wifi rede3
  Ipv4InterfaceContainer sta3If = address.Assign (staDev3);
  Ipv4InterfaceContainer ap3If  = address.Assign (apDev3);

  // Popula tabelas de roteamento global
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Imprime algumas informações para depuração
  std::cout << "AP1 address: " << ap1If.GetAddress (0) << std::endl;
  std::cout << "AP2 address: " << ap2If.GetAddress (0) << std::endl;
  std::cout << "AP3 address: " << ap3If.GetAddress (0) << std::endl;
  std::cout << "Sample STA1[0] address: " << sta1If.GetAddress (0) << std::endl;
  std::cout << "Sample STA2[0] address: " << sta2If.GetAddress (0) << std::endl;
  std::cout << "Sample STA3[0] address: " << sta3If.GetAddress (0) << std::endl;

  // Habilita pcap nos APs para depurar o rádio
  if (tracing)
    {
      phy1.EnablePcap ("ap1-phy", apDev1.Get (0));
      phy2.EnablePcap ("ap2-phy", apDev2.Get (0));
      phy3.EnablePcap ("ap3-phy", apDev3.Get (0));
      p2p.EnablePcapAll ("p2p-ap-links");
    }

  // Aplicação de teste: UDP echo server na primeira STA da rede3 (como exemplo)
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (staNet3.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (60.0));

  // Cliente na rede1 (primeira STA)
  UdpEchoClientHelper echoClient (sta3If.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApps = echoClient.Install (staNet1.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (60.0));

  Simulator::Stop (Seconds (65.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}

