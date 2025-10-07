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

NS_LOG_COMPONENT_DEFINE("ThreeWifiNetworks");

int main(int argc, char *argv[]) {
    LogComponentEnable("Ping", LOG_LEVEL_INFO);

    uint32_t nWifi = 3;
    bool tracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifi", "Number of STA nodes per Wi-Fi network", nWifi);
    cmd.Parse(argc, argv);

    // === Nós principais (APs) ===
    NodeContainer apNodes;
    apNodes.Create(3); // AP1, AP2, AP3

    // === Enlaces P2P entre APs ===
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2 = p2p.Install(apNodes.Get(0), apNodes.Get(1));
    NetDeviceContainer ap1ap3 = p2p.Install(apNodes.Get(0), apNodes.Get(2));
    NetDeviceContainer ap2ap3 = p2p.Install(apNodes.Get(1), apNodes.Get(2));

    // === Rede CSMA ===
    NodeContainer csmaNodes;
    csmaNodes.Add(apNodes.Get(1)); // AP2 conectado ao CSMA
    csmaNodes.Create(3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    // === Três redes Wi-Fi ===
    NodeContainer wifiStaNodes1, wifiStaNodes2, wifiStaNodes3;
    wifiStaNodes1.Create(nWifi);
    wifiStaNodes2.Create(nWifi);
    wifiStaNodes3.Create(nWifi);

    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();

    YansWifiPhyHelper phy1, phy2, phy3;
    phy1.SetChannel(channel1.Create());
    phy2.SetChannel(channel2.Create());
    phy3.SetChannel(channel3.Create());

    WifiHelper wifi;
    WifiMacHelper mac;

    Ssid ssid1 = Ssid("wifi-1");
    Ssid ssid2 = Ssid("wifi-2");
    Ssid ssid3 = Ssid("wifi-3");

    // --- WiFi 1 ---
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy1, mac, wifiStaNodes1);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevice1 = wifi.Install(phy1, mac, apNodes.Get(0));

    // --- WiFi 2 ---
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevice2 = wifi.Install(phy2, mac, apNodes.Get(1));

    // --- WiFi 3 ---
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    NetDeviceContainer apDevice3 = wifi.Install(phy3, mac, apNodes.Get(2));

    // === Mobilidade ===
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(wifiStaNodes1);
    mobility.Install(wifiStaNodes2);
    mobility.Install(wifiStaNodes3);

    // === Pilha IPv6 ===
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(wifiStaNodes1);
    stack.Install(wifiStaNodes2);
    stack.Install(wifiStaNodes3);
    stack.Install(csmaNodes);

    Ipv6AddressHelper address;
    Ipv6StaticRoutingHelper ipv6RoutingHelper;

    // Atribuição de endereços IPv6
    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap1ap2If = address.Assign(ap1ap2);

    address.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap1ap3If = address.Assign(ap1ap3);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap2ap3If = address.Assign(ap2ap3);

    address.SetBase(Ipv6Address("2001:10::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIf1 = address.Assign(staDevices1);
    Ipv6InterfaceContainer apIf1 = address.Assign(apDevice1);

    address.SetBase(Ipv6Address("2001:20::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIf2 = address.Assign(staDevices2);
    Ipv6InterfaceContainer apIf2 = address.Assign(apDevice2);

    address.SetBase(Ipv6Address("2001:30::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIf3 = address.Assign(staDevices3);
    Ipv6InterfaceContainer apIf3 = address.Assign(apDevice3);

    address.SetBase(Ipv6Address("2001:40::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer csmaIf = address.Assign(csmaDevices);

    // === Rotas estáticas ===
    for (uint32_t i = 0; i < wifiStaNodes1.GetN(); i++) {
        Ptr<Ipv6> ipv6 = wifiStaNodes1.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(apIf1.GetAddress(0, 1), ipv6->GetInterfaceForDevice(staDevices1.Get(i)));
    }
    for (uint32_t i = 0; i < wifiStaNodes2.GetN(); i++) {
        Ptr<Ipv6> ipv6 = wifiStaNodes2.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(apIf2.GetAddress(0, 1), ipv6->GetInterfaceForDevice(staDevices2.Get(i)));
    }
    for (uint32_t i = 0; i < wifiStaNodes3.GetN(); i++) {
        Ptr<Ipv6> ipv6 = wifiStaNodes3.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6RoutingHelper.GetStaticRouting(ipv6);
        sr->SetDefaultRoute(apIf3.GetAddress(0, 1), ipv6->GetInterfaceForDevice(staDevices3.Get(i)));
    }

    // === Aplicação Ping ===
    PingHelper ping(csmaIf.GetAddress(0, 1)); // destino: CSMA
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(512));
    ping.SetAttribute("Count", UintegerValue(5));

    ApplicationContainer pingApp = ping.Install(wifiStaNodes1.Get(0));
    pingApp.Start(Seconds(30.0));
    pingApp.Stop(Seconds(60.0));

    // === Rastreamento ===
    if (tracing) {
        phy1.EnablePcap("wifi1", apDevice1.Get(0));
        phy2.EnablePcap("wifi2", apDevice2.Get(0));
        phy3.EnablePcap("wifi3", apDevice3.Get(0));
        p2p.EnablePcapAll("p2p-links");
        csma.EnablePcap("csma", csmaDevices.Get(0), true);
    }

    Simulator::Stop(Seconds(120.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

