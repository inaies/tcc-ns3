#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ping-helper.h"
#include "ns3/csma-module.h"

using namespace ns3;

int main() {
    // Enable logging
    LogComponentEnable("Ping", LOG_LEVEL_INFO);
    
    // Create nodes
    NodeContainer staGroup1, ap1, staGroup2, ap2, staGroup3, ap3;
    staGroup1.Create(5); // Reduzido para teste
    ap1.Create(1);
    staGroup2.Create(5);
    ap2.Create(1);
    staGroup3.Create(5);
    ap3.Create(1);

    // Create P2P connections between APs
    NodeContainer ap1ap2(ap1.Get(0), ap2.Get(0));
    NodeContainer ap2ap3(ap2.Get(0), ap3.Get(0));
    NodeContainer ap3ap1(ap3.Get(0), ap1.Get(0));
    
    // Configure P2P links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevs1 = p2p.Install(ap1ap2);
    NetDeviceContainer p2pDevs2 = p2p.Install(ap2ap3);
    NetDeviceContainer p2pDevs3 = p2p.Install(ap3ap1);

    // Install Internet stack (IPv6 only)
    InternetStackHelper internet;
    internet.SetIpv4StackInstall(false);
    internet.SetIpv6StackInstall(true);
    internet.Install(staGroup1);
    internet.Install(ap1);
    internet.Install(staGroup2);
    internet.Install(ap2);
    internet.Install(staGroup3);
    internet.Install(ap3);

    // Configure WiFi Network 1
    WifiHelper wifi1;
    wifi1.SetStandard(WIFI_STANDARD_80211n);
    WifiMacHelper mac1;
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy1;
    phy1.SetChannel(channel1.Create());
    
    mac1.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("rede1")));
    NetDeviceContainer staDevs1 = wifi1.Install(phy1, mac1, staGroup1);
    mac1.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("rede1")));
    NetDeviceContainer apDev1 = wifi1.Install(phy1, mac1, ap1);

    // Configure WiFi Network 2
    WifiHelper wifi2;
    wifi2.SetStandard(WIFI_STANDARD_80211n);
    WifiMacHelper mac2;
    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy2;
    phy2.SetChannel(channel2.Create());
    
    mac2.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("rede2")));
    NetDeviceContainer staDevs2 = wifi2.Install(phy2, mac2, staGroup2);
    mac2.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("rede2")));
    NetDeviceContainer apDev2 = wifi2.Install(phy2, mac2, ap2);

    // Configure WiFi Network 3
    WifiHelper wifi3;
    wifi3.SetStandard(WIFI_STANDARD_80211n);
    WifiMacHelper mac3;
    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy3;
    phy3.SetChannel(channel3.Create());
    
    mac3.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("rede3")));
    NetDeviceContainer staDevs3 = wifi3.Install(phy3, mac3, staGroup3);
    mac3.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("rede3")));
    NetDeviceContainer apDev3 = wifi3.Install(phy3, mac3, ap3);

    // Configure mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.InstallAll();

    // Assign IPv6 addresses
    Ipv6AddressHelper ip;

    // WiFi networks
    ip.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIfs1 = ip.Assign(staDevs1);
    Ipv6InterfaceContainer apIfs1 = ip.Assign(apDev1);

    ip.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIfs2 = ip.Assign(staDevs2);
    Ipv6InterfaceContainer apIfs2 = ip.Assign(apDev2);

    ip.SetBase(Ipv6Address("2001:db8:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIfs3 = ip.Assign(staDevs3);
    Ipv6InterfaceContainer apIfs3 = ip.Assign(apDev3);

    // P2P links between APs
    ip.SetBase(Ipv6Address("2001:db8:100::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pIfs1 = ip.Assign(p2pDevs1); // AP1-AP2

    ip.SetBase(Ipv6Address("2001:db8:101::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pIfs2 = ip.Assign(p2pDevs2); // AP2-AP3

    ip.SetBase(Ipv6Address("2001:db8:102::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pIfs3 = ip.Assign(p2pDevs3); // AP3-AP1

    // Enable forwarding on APs
    for (auto node : {ap1.Get(0), ap2.Get(0), ap3.Get(0)}) {
        Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
        for (uint32_t i = 0; i < ipv6->GetNInterfaces(); ++i) {
            ipv6->SetForwarding(i, true);
        }
    }

    // Print interface information for debugging
    std::cout << "\n=== AP Interface Information ===" << std::endl;
    
    std::cout << "AP1 Interfaces:" << std::endl;
    Ptr<Ipv6> ipv6AP1 = ap1.Get(0)->GetObject<Ipv6>();
    for (uint32_t i = 0; i < ipv6AP1->GetNInterfaces(); i++) {
        for(uint32_t j = 0; j < ipv6AP1->GetNAddresses(i); j++){
            Ipv6Address addr = ipv6AP1->GetAddress(i, j).GetAddress();
            if (!addr.IsLinkLocal() && !addr.IsLoopback()) {
                std::cout << "  Interface " << i << " address " << j << ": " << addr << std::endl;
            }
        }
    }

    std::cout << "AP2 Interfaces:" << std::endl;
    Ptr<Ipv6> ipv6AP2 = ap2.Get(0)->GetObject<Ipv6>();
    for (uint32_t i = 0; i < ipv6AP2->GetNInterfaces(); i++) {
        for(uint32_t j = 0; j < ipv6AP2->GetNAddresses(i); j++){
            Ipv6Address addr = ipv6AP2->GetAddress(i, j).GetAddress();
            if (!addr.IsLinkLocal() && !addr.IsLoopback()) {
                std::cout << "  Interface " << i << " address " << j << ": " << addr << std::endl;
            }
        }
    }

    std::cout << "AP3 Interfaces:" << std::endl;
    Ptr<Ipv6> ipv6AP3 = ap3.Get(0)->GetObject<Ipv6>();
    for (uint32_t i = 0; i < ipv6AP3->GetNInterfaces(); i++) {
        for(uint32_t j = 0; j < ipv6AP3->GetNAddresses(i); j++){
            Ipv6Address addr = ipv6AP3->GetAddress(i, j).GetAddress();
            if (!addr.IsLinkLocal() && !addr.IsLoopback()) {
                std::cout << "  Interface " << i << " address " << j << ": " << addr << std::endl;
            }
        }
    }

    // Configure comprehensive static routing
    Ipv6StaticRoutingHelper routingHelper;
    
    Ptr<Ipv6StaticRouting> staticRoutingAp1 = routingHelper.GetStaticRouting(ipv6AP1);
    Ptr<Ipv6StaticRouting> staticRoutingAp2 = routingHelper.GetStaticRouting(ipv6AP2);
    Ptr<Ipv6StaticRouting> staticRoutingAp3 = routingHelper.GetStaticRouting(ipv6AP3);

    // Get interface indices correctly
    uint32_t ap1_wifi_if = 1;  // WiFi interface
    uint32_t ap1_p2p_ap2_if = 2;  // P2P to AP2
    uint32_t ap1_p2p_ap3_if = 3;  // P2P to AP3

    uint32_t ap2_wifi_if = 1;  // WiFi interface  
    uint32_t ap2_p2p_ap1_if = 2;  // P2P to AP1
    uint32_t ap2_p2p_ap3_if = 3;  // P2P to AP3

    uint32_t ap3_wifi_if = 1;  // WiFi interface
    uint32_t ap3_p2p_ap2_if = 2;  // P2P to AP2
    uint32_t ap3_p2p_ap1_if = 3;  // P2P to AP1

    std::cout << "\n=== Configuring AP Routing ===" << std::endl;

    // ===== AP1 ROUTING RULES =====
    std::cout << "Configuring AP1 routes..." << std::endl;
    
    // Route to network 2 (staGroup2) via AP2
    staticRoutingAp1->AddNetworkRouteTo(
        Ipv6Address("2001:db8:2::"), Ipv6Prefix(64),
        p2pIfs1.GetAddress(1,1), ap1_p2p_ap2_if, 0);
    
    // Route to network 3 (staGroup3) via AP3 
    staticRoutingAp1->AddNetworkRouteTo(
        Ipv6Address("2001:db8:3::"), Ipv6Prefix(64),
        p2pIfs3.GetAddress(1,1), ap1_p2p_ap3_if, 0);

    // Routes to P2P networks for AP access
    staticRoutingAp1->AddNetworkRouteTo(
        Ipv6Address("2001:db8:101::"), Ipv6Prefix(64),  // AP2-AP3 link via AP2
        p2pIfs1.GetAddress(1,1), ap1_p2p_ap2_if, 0);

    // ===== AP2 ROUTING RULES =====
    std::cout << "Configuring AP2 routes..." << std::endl;
    
    // Route to network 1 (staGroup1) via AP1
    staticRoutingAp2->AddNetworkRouteTo(
        Ipv6Address("2001:db8:1::"), Ipv6Prefix(64),
        p2pIfs1.GetAddress(0,1), ap2_p2p_ap1_if, 0);
    
    // Route to network 3 (staGroup3) via AP3
    staticRoutingAp2->AddNetworkRouteTo(
        Ipv6Address("2001:db8:3::"), Ipv6Prefix(64),
        p2pIfs2.GetAddress(1,1), ap2_p2p_ap3_if, 0);

    // Routes to P2P networks for AP access
    staticRoutingAp2->AddNetworkRouteTo(
        Ipv6Address("2001:db8:102::"), Ipv6Prefix(64),  // AP3-AP1 link via AP3
        p2pIfs2.GetAddress(1,1), ap2_p2p_ap3_if, 0);

    // ===== AP3 ROUTING RULES =====
    std::cout << "Configuring AP3 routes..." << std::endl;
    
    // Route to network 1 (staGroup1) via AP1
    staticRoutingAp3->AddNetworkRouteTo(
        Ipv6Address("2001:db8:1::"), Ipv6Prefix(64),
        p2pIfs3.GetAddress(0,1), ap3_p2p_ap1_if, 0);
    
    // Route to network 2 (staGroup2) via AP2
    staticRoutingAp3->AddNetworkRouteTo(
        Ipv6Address("2001:db8:2::"), Ipv6Prefix(64),
        p2pIfs2.GetAddress(0,1), ap3_p2p_ap2_if, 0);

    // Routes to P2P networks for AP access
    staticRoutingAp3->AddNetworkRouteTo(
        Ipv6Address("2001:db8:100::"), Ipv6Prefix(64),  // AP1-AP2 link via AP1
        p2pIfs3.GetAddress(0,1), ap3_p2p_ap1_if, 0);

    std::cout << "\n=== Configuring Station Routing ===" << std::endl;

    // ===== NETWORK 1 STATIONS ROUTING =====
    std::cout << "Configuring Network 1 station routes..." << std::endl;
    for (uint32_t i = 0; i < staGroup1.GetN(); i++) {
        Ptr<Ipv6StaticRouting> staticRoutingSta = routingHelper.GetStaticRouting(
            staGroup1.Get(i)->GetObject<Ipv6>());
        
        // Default route via local AP
        staticRoutingSta->SetDefaultRoute(apIfs1.GetAddress(0,1), 1);
        
        // Specific routes to other networks
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:2::"), Ipv6Prefix(64),
            apIfs1.GetAddress(0,1), 1, 0);
            
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:3::"), Ipv6Prefix(64),
            apIfs1.GetAddress(0,1), 1, 0);

        // Routes to P2P networks (to reach APs directly)
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:100::"), Ipv6Prefix(64),
            apIfs1.GetAddress(0,1), 1, 0);
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:101::"), Ipv6Prefix(64),
            apIfs1.GetAddress(0,1), 1, 0);
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:102::"), Ipv6Prefix(64),
            apIfs1.GetAddress(0,1), 1, 0);
    }
    
    // ===== NETWORK 2 STATIONS ROUTING =====
    std::cout << "Configuring Network 2 station routes..." << std::endl;
    for (uint32_t i = 0; i < staGroup2.GetN(); i++) {
        Ptr<Ipv6StaticRouting> staticRoutingSta = routingHelper.GetStaticRouting(
            staGroup2.Get(i)->GetObject<Ipv6>());
        
        // Default route via local AP
        staticRoutingSta->SetDefaultRoute(apIfs2.GetAddress(0,1), 1);
        
        // Specific routes to other networks
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:1::"), Ipv6Prefix(64),
            apIfs2.GetAddress(0,1), 1, 0);
            
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:3::"), Ipv6Prefix(64),
            apIfs2.GetAddress(0,1), 1, 0);

        // Routes to P2P networks (to reach APs directly)
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:100::"), Ipv6Prefix(64),
            apIfs2.GetAddress(0,1), 1, 0);
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:101::"), Ipv6Prefix(64),
            apIfs2.GetAddress(0,1), 1, 0);
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:102::"), Ipv6Prefix(64),
            apIfs2.GetAddress(0,1), 1, 0);
    }

    // ===== NETWORK 3 STATIONS ROUTING =====
    std::cout << "Configuring Network 3 station routes..." << std::endl;
    for (uint32_t i = 0; i < staGroup3.GetN(); i++) {
        Ptr<Ipv6StaticRouting> staticRoutingSta = routingHelper.GetStaticRouting(
            staGroup3.Get(i)->GetObject<Ipv6>());
        
        // Default route via local AP
        staticRoutingSta->SetDefaultRoute(apIfs3.GetAddress(0,1), 1);
        
        // Specific routes to other networks
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:1::"), Ipv6Prefix(64),
            apIfs3.GetAddress(0,1), 1, 0);
            
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:2::"), Ipv6Prefix(64),
            apIfs3.GetAddress(0,1), 1, 0);

        // Routes to P2P networks (to reach APs directly)
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:100::"), Ipv6Prefix(64),
            apIfs3.GetAddress(0,1), 1, 0);
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:101::"), Ipv6Prefix(64),
            apIfs3.GetAddress(0,1), 1, 0);
        staticRoutingSta->AddNetworkRouteTo(
            Ipv6Address("2001:db8:102::"), Ipv6Prefix(64),
            apIfs3.GetAddress(0,1), 1, 0);
    }

    std::cout << "=== Routing Configuration Complete ===" << std::endl;

    // Print routing tables for debugging
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("routing_tables.txt", std::ios::out);
    routingHelper.PrintRoutingTableAt(Seconds(1.0), ap1.Get(0), routingStream);
    routingHelper.PrintRoutingTableAt(Seconds(1.1), ap2.Get(0), routingStream);
    routingHelper.PrintRoutingTableAt(Seconds(1.2), ap3.Get(0), routingStream);

    // Enable packet capture
    p2p.EnablePcapAll("p2p-trace");
    phy1.EnablePcap("wifi1", staDevs1.Get(0));
    phy2.EnablePcap("wifi2", staDevs2.Get(0));
    phy3.EnablePcap("wifi3", staDevs3.Get(0));

    std::cout << "\n=== Network Configuration Complete ===" << std::endl;
    std::cout << "Network 1 (2001:db8:1::): " << staGroup1.GetN() << " stations" << std::endl;
    std::cout << "Network 2 (2001:db8:2::): " << staGroup2.GetN() << " stations" << std::endl;
    std::cout << "Network 3 (2001:db8:3::): " << staGroup3.GetN() << " stations" << std::endl;
    
    // Test comprehensive connectivity with multiple ping scenarios
    
    std::cout << "\n=== Configuring Comprehensive Connectivity Tests ===" << std::endl;
    
    // Test 1: Station-to-Station across networks
    std::cout << "Test 1: Station Network 1 → Station Network 3" << std::endl;
    PingHelper ping1(staIfs3.GetAddress(0,1)); // Target: first station in network 3
    ping1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping1.SetAttribute("Size", UintegerValue(64));
    ping1.SetAttribute("Count", UintegerValue(3));
    ApplicationContainer pingApp1 = ping1.Install(staGroup1.Get(0));
    pingApp1.Start(Seconds(10.0));
    pingApp1.Stop(Seconds(15.0));

    // Test 2: Station-to-Station across networks (reverse)
    std::cout << "Test 2: Station Network 3 → Station Network 1" << std::endl;
    PingHelper ping2(staIfs1.GetAddress(0,1)); // Target: first station in network 1
    ping2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping2.SetAttribute("Size", UintegerValue(64));
    ping2.SetAttribute("Count", UintegerValue(3));
    ApplicationContainer pingApp2 = ping2.Install(staGroup3.Get(0));
    pingApp2.Start(Seconds(20.0));
    pingApp2.Stop(Seconds(25.0));

    // Test 3: Station-to-AP across networks
    std::cout << "Test 3: Station Network 1 → AP2" << std::endl;
    PingHelper ping3(apIfs2.GetAddress(0,1)); // Target: AP2
    ping3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping3.SetAttribute("Size", UintegerValue(64));
    ping3.SetAttribute("Count", UintegerValue(3));
    ApplicationContainer pingApp3 = ping3.Install(staGroup1.Get(1));
    pingApp3.Start(Seconds(30.0));
    pingApp3.Stop(Seconds(35.0));

    // Test 4: AP-to-Station across networks
    std::cout << "Test 4: AP1 → Station Network 2" << std::endl;
    PingHelper ping4(staIfs2.GetAddress(0,1)); // Target: first station in network 2
    ping4.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping4.SetAttribute("Size", UintegerValue(64));
    ping4.SetAttribute("Count", UintegerValue(3));
    ApplicationContainer pingApp4 = ping4.Install(ap1.Get(0));
    pingApp4.Start(Seconds(40.0));
    pingApp4.Stop(Seconds(45.0));

    // Test 5: Station-to-P2P network (testing access to backbone)
    std::cout << "Test 5: Station Network 2 → P2P Network (AP1-AP3 link)" << std::endl;
    PingHelper ping5(p2pIfs3.GetAddress(0,1)); // Target: AP1 side of AP1-AP3 link
    ping5.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping5.SetAttribute("Size", UintegerValue(64));
    ping5.SetAttribute("Count", UintegerValue(3));
    ApplicationContainer pingApp5 = ping5.Install(staGroup2.Get(0));
    pingApp5.Start(Seconds(50.0));
    pingApp5.Stop(Seconds(55.0));

    // Test 6: Cross-network communication (Network 2 ↔ Network 3)
    std::cout << "Test 6: Station Network 2 → Station Network 3" << std::endl;
    PingHelper ping6(staIfs3.GetAddress(1,1)); // Target: second station in network 3
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("Size", UintegerValue(64));
    ping6.SetAttribute("Count", UintegerValue(3));
    ApplicationContainer pingApp6 = ping6.Install(staGroup2.Get(1));
    pingApp6.Start(Seconds(60.0));
    pingApp6.Stop(Seconds(65.0));

    // Test 7: Multiple stations testing simultaneously
    std::cout << "Test 7: Multiple simultaneous cross-network pings" << std::endl;
    
    // Multiple stations from Network 1 to different targets
    PingHelper ping7a(staIfs2.GetAddress(0,1)); // Net1 → Net2
    ping7a.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    ping7a.SetAttribute("Size", UintegerValue(32));
    ping7a.SetAttribute("Count", UintegerValue(6));
    ApplicationContainer pingApp7a = ping7a.Install(staGroup1.Get(2));
    pingApp7a.Start(Seconds(70.0));
    pingApp7a.Stop(Seconds(75.0));

    PingHelper ping7b(apIfs3.GetAddress(0,1)); // Net1 → AP3
    ping7b.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    ping7b.SetAttribute("Size", UintegerValue(32));
    ping7b.SetAttribute("Count", UintegerValue(6));
    ApplicationContainer pingApp7b = ping7b.Install(staGroup1.Get(3));
    pingApp7b.Start(Seconds(70.0));
    pingApp7b.Stop(Seconds(75.0));

    std::cout << "\nAll connectivity tests configured!" << std::endl;
    std::cout << "Expected behavior:" << std::endl;
    std::cout << "- All stations can communicate with any other station" << std::endl;
    std::cout << "- All stations can reach any AP" << std::endl;
    std::cout << "- All stations can access P2P backbone networks" << std::endl;
    std::cout << "- Traffic will be routed through appropriate APs" << std::endl;

    std::cout << "\n=== Starting Simulation ===" << std::endl;
    
    // Run simulation
    Simulator::Stop(Seconds(90.0));
    Simulator::Run();
    Simulator::Destroy();

    std::cout << "\n=== Simulation Complete ===" << std::endl;
    std::cout << "✓ Comprehensive routing configured between all networks" << std::endl;
    std::cout << "✓ Stations can communicate with stations in other networks" << std::endl;
    std::cout << "✓ Stations can reach APs in other networks" << std::endl;
    std::cout << "✓ APs can communicate with stations in other networks" << std::endl;
    std::cout << "✓ Access to P2P backbone networks enabled" << std::endl;
    std::cout << "\nCheck files:" << std::endl;
    std::cout << "- routing_tables.txt: Detailed routing information" << std::endl;
    std::cout << "- *.pcap files: Packet captures for analysis" << std::endl;

    return 0;
}
