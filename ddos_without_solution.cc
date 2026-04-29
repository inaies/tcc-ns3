// ddos_unmitigated.cc
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv6-flow-classifier.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ping-helper.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ripng-helper.h"

#include <cmath>
#include <iostream>

NS_LOG_COMPONENT_DEFINE("DdosUnmitigated");

using namespace ns3;

static NodeContainer wifiStaNodes1; 
static NodeContainer wifiStaNodes2;
static NodeContainer wifiStaNodes3; 

// Relógio para sabermos que a simulação não travou
void PrintProgress()
{
    std::cout << "[STATUS] Simulação a decorrer... Tempo atual: " 
              << Simulator::Now().GetSeconds() << "s / 900s" << std::endl;
    Simulator::Schedule(Seconds(50.0), &PrintProgress);
}

static Ptr<ListPositionAllocator>
CreateGridPositionAllocator (uint32_t nNodes, double spacing, double offsetX, double offsetY)
{
  Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator> ();
  if (nNodes == 0) return allocator;
  uint32_t cols = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(nNodes))));
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

int main(int argc, char* argv[])
{
    std::cout << "==========================================" << std::endl;
    std::cout << " INICIANDO CENÁRIO: ATAQUE SEM MITIGAÇÃO  " << std::endl;
    std::cout << "==========================================" << std::endl;

    LogComponentEnable("Ping", LOG_LEVEL_INFO);
    LogComponentEnable("DdosUnmitigated", LOG_LEVEL_INFO);

    // =======================================================
    // LIMITAÇÃO DE BUFFER - REGRA "CACHINHOS DE OURO" (50ms)
    // =======================================================
    Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("5p"));
    Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(MilliSeconds(50)));

    bool verbose = true;
    uint32_t nWifiCsma = 173; 
    uint32_t nWifi = 173;
    bool tracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    wifiStaNodes1.Create(nWifi);
    wifiStaNodes2.Create(nWifi);
    wifiStaNodes3.Create(nWifi);

    NodeContainer p2pNodes;
    p2pNodes.Create(3); // n0=AP1, n1=AP2, n2=AP3

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2, ap1ap3, ap2ap3;
    ap1ap2 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(1));
    ap1ap3 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));
    ap2ap3 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));

    NodeContainer wifiApNode  = p2pNodes.Get(0); 
    NodeContainer wifiApNode2 = p2pNodes.Get(1); 
    NodeContainer wifiApNode3 = p2pNodes.Get(2); 

    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy1; phy1.SetChannel(channel1.Create());
    phy1.Set("ChannelSettings", StringValue("{36, 0, BAND_5GHZ, 0}"));

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy2; phy2.SetChannel(channel2.Create());
    phy2.Set("ChannelSettings", StringValue("{40, 0, BAND_5GHZ, 0}"));

    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy3; phy3.SetChannel(channel3.Create());
    phy3.Set("ChannelSettings", StringValue("{44, 0, BAND_5GHZ, 0}"));

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");
    Ssid ssid3 = Ssid("ns-3-ssid-3");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy1, mac, wifiStaNodes1);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifi.Install(phy1, mac, wifiApNode);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    NetDeviceContainer apDevices3 = wifi.Install(phy3, mac, wifiApNode3);

    MobilityHelper mobility;
    double spacing = 5.0;    
    double offsetCell = 75.0; 

    Ptr<ListPositionAllocator> allocWifi1 = CreateGridPositionAllocator (nWifi, spacing, 0.0, 0.0);
    Ptr<ListPositionAllocator> allocWifi2 = CreateGridPositionAllocator (nWifi, spacing, 0.0, offsetCell);    
    Ptr<ListPositionAllocator> allocWifi3 = CreateGridPositionAllocator (nWifiCsma, spacing, offsetCell, 0.0);  

    mobility.SetPositionAllocator (allocWifi1);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes1);

    mobility.SetPositionAllocator (allocWifi2);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes2);

    mobility.SetPositionAllocator (allocWifi3);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes3);

    Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator> ();
    uint32_t cols1 = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(std::max<uint32_t>(1, nWifi)))));
    uint32_t cols3 = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(std::max<uint32_t>(1, nWifiCsma)))));

    double ap1x = (cols1 * spacing) / 2.0;
    double ap1y = (cols1 * spacing) / 2.0;
    apAlloc->Add (Vector (ap1x, ap1y, 0.0));

    double ap2x = (cols1 * spacing) / 2.0;
    double ap2y = offsetCell + (cols1 * spacing) / 2.0;
    apAlloc->Add (Vector (ap2x, ap2y, 0.0));
    
    double ap3x = offsetCell + (cols3 * spacing) / 2.0;
    double ap3y = (cols3 * spacing) / 2.0;
    apAlloc->Add (Vector (ap3x, ap3y, 0.0));

    mobility.SetPositionAllocator (apAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNode);  
    mobility.Install (wifiApNode2); 
    mobility.Install (wifiApNode3); 

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

    Ipv6AddressHelper address;
    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64)); 
    Ipv6InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64)); 
    Ipv6InterfaceContainer wifiInterfaces1 = address.Assign(staDevices1);
    Ipv6InterfaceContainer apInterfaces1   = address.Assign(apDevices1);

    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64)); 
    Ipv6InterfaceContainer wifiInterfaces2 = address.Assign(staDevices2);
    Ipv6InterfaceContainer apInterfaces2   = address.Assign(apDevices2);

    address.SetBase(Ipv6Address("2001:7::"), Ipv6Prefix(64)); 
    Ipv6InterfaceContainer wifiInterfaces3 = address.Assign(staDevices3);
    Ipv6InterfaceContainer apInterfaces3   = address.Assign(apDevices3);

    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64)); 
    Ipv6InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64)); 
    Ipv6InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);

    for (uint32_t i = 0; i < p2pNodes.GetN(); ++i) {
        Ptr<Ipv6> ipv6 = p2pNodes.Get(i)->GetObject<Ipv6>();
        ipv6->SetForwarding(0, true);
    }

    Ipv6Address ap1Addr = apInterfaces1.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes1.GetN(); i++) {
        Ptr<Ipv6> ipv6 = wifiStaNodes1.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(ap1Addr, ipv6->GetInterfaceForDevice(staDevices1.Get(i)));
    }

    Ipv6Address ap2Addr = apInterfaces2.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes2.GetN(); i++) {
        Ptr<Ipv6> ipv6 = wifiStaNodes2.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(ap2Addr, ipv6->GetInterfaceForDevice(staDevices2.Get(i)));
    }

    Ipv6Address ap3Addr = apInterfaces3.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes3.GetN(); i++) {
        Ptr<Ipv6> ipv6 = wifiStaNodes3.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(ap3Addr, ipv6->GetInterfaceForDevice(staDevices3.Get(i)));
    }

    // --------------------------------------------------------------------------------
    // RECEPTOR (SINK) COMUM NO ROTEADOR AP2
    // --------------------------------------------------------------------------------
    Ptr<Node> ap2_receptor = wifiApNode2.Get(0); 
    uint16_t sinkPort = 9002;
    
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", Inet6SocketAddress(Ipv6Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(ap2_receptor);
    sinkApp.Start(Seconds(1.5)); 
    sinkApp.Stop(Seconds(900.0));

    // ==========================================
    // TRÁFEGO COMUM (Idêntico ao cenário OpenGym)
    // ==========================================
    Ipv6Address ap2_address = apInterfaces2.GetAddress(0, 1); 

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(Inet6SocketAddress(ap2_address, sinkPort)));
    onoff.SetAttribute("DataRate", StringValue("16kbps")); 
    onoff.SetAttribute("PacketSize", UintegerValue(1000)); 
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); 
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    uv->SetAttribute("Min", DoubleValue(0.0));
    uv->SetAttribute("Max", DoubleValue(2.0));

    for (uint32_t i = 21; i < wifiStaNodes2.GetN(); i++) {
      ApplicationContainer clientApp = onoff.Install(wifiStaNodes2.Get(i));
      double start_time = 1.0 + uv->GetValue();
      clientApp.Start(Seconds(start_time));
      clientApp.Stop(Seconds(900.0)); 
    }

    // ==========================================
    // ATAQUE DDOS (SEM DEFESA)
    // ==========================================
    NodeContainer attackerNodesWave1;
    for (int i = 0; i < 10; i++) attackerNodesWave1.Add(wifiStaNodes2.Get(i));

    NodeContainer attackerNodesWave2;
    for (int i = 10; i < 20; i++) attackerNodesWave2.Add(wifiStaNodes2.Get(i));

    uint16_t attackPort = 9001;
    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory", Inet6SocketAddress(Ipv6Address::GetAny(), attackPort));
    ApplicationContainer sinkAppAttack = udpSinkHelper.Install(ap2_receptor);
    sinkAppAttack.Start(Seconds(1.0));
    sinkAppAttack.Stop(Seconds(900.0));
  
    // ONDA 1: 155s até 200s
    OnOffHelper onoffWave1("ns3::UdpSocketFactory", Address(Inet6SocketAddress(ap2_address, attackPort)));
    onoffWave1.SetAttribute("DataRate", StringValue("100Mbps")); 
    onoffWave1.SetAttribute("PacketSize", UintegerValue(64));
    onoffWave1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=15]"));
    onoffWave1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    for (uint32_t i = 0; i < attackerNodesWave1.GetN(); i++) {
        ApplicationContainer attackApp1 = onoffWave1.Install(attackerNodesWave1.Get(i));
        double start_time = 155.0 + (i * 0.05); 
        attackApp1.Start(Seconds(start_time));
        attackApp1.Stop(Seconds(200.0));
    }

    // ONDA 2: 250s até 300s
    OnOffHelper onoffWave2("ns3::UdpSocketFactory", Address(Inet6SocketAddress(ap2_address, attackPort)));
    onoffWave2.SetAttribute("DataRate", StringValue("100Mbps")); 
    onoffWave2.SetAttribute("PacketSize", UintegerValue(64));
    onoffWave2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=15]"));
    onoffWave2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    for (uint32_t i = 0; i < attackerNodesWave2.GetN(); i++) {
        ApplicationContainer attackApp2 = onoffWave2.Install(attackerNodesWave2.Get(i));
        double start_time = 250.0 + (i * 0.05); 
        attackApp2.Start(Seconds(start_time));
        attackApp2.Stop(Seconds(300.0));
    }

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    if (tracing) {
        // Altera o nome do PCAP para não sobrescrever o ficheiro da IA
        pointToPoint.EnablePcapAll("p2p-traffic-ddos_without_solution");
        phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy2.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy3.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

        phy1.EnablePcap("ddos_without_solution_ap1", apDevices1.Get(0)); 
        phy2.EnablePcap("ddos_without_solution_ap2", apDevices2.Get(0)); 
        phy3.EnablePcap("ddos_without_solution_ap3", apDevices3.Get(0)); 
    }
    
    // Inicia a barra de progresso no terminal
    Simulator::Schedule(Seconds(0.0), &PrintProgress);

    Simulator::Stop(Seconds(901.0));
    Simulator::Run();
    
    flowMonitor->SerializeToXmlFile("ddos_without_solution-flowmon.xml", true, true);
    
    Simulator::Destroy();
    std::cout << "-> PROCESSO FINALIZADO COM SUCESSO." << std::endl;
    return 0;
}