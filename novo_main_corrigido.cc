#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ping-helper.h"
#include <ns3/lr-wpan-module.h>

using namespace ns3;

int main() {

    LogComponentEnable("Ping", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    
    NodeContainer staGroup1, ap1, staGroup2, ap2, staGroup3, ap3;
    staGroup1.Create(50);
    ap1.Create(1);
    staGroup2.Create(50);
    ap2.Create(1);
    staGroup3.Create(50);
    ap3.Create(1);

    NodeContainer ap1ap2(ap1.Get(0), ap2.Get(0));
    NodeContainer ap2ap3(ap2.Get(0), ap3.Get(0));
    NodeContainer ap3ap1(ap3.Get(0), ap1.Get(0));
    
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevs1 = p2p.Install(ap1ap2);
    NetDeviceContainer p2pDevs2 = p2p.Install(ap2ap3);
    NetDeviceContainer p2pDevs3 = p2p.Install(ap3ap1);

    InternetStackHelper internet;
    // internet.SetIpv6StackInstall(true);
    internet.SetIpv4StackInstall(false);
    internet.Install(staGroup1);
    internet.Install(ap1);
    internet.Install(staGroup2);
    internet.Install(ap2);
    internet.Install(staGroup3);
    internet.Install(ap3);

    // Rede 1
    WifiHelper wifi1;
    wifi1.SetStandard(WIFI_STANDARD_80211n);
    WifiMacHelper mac1;
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy1;
    phy1.SetChannel(channel1.Create());
    mac1.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("rede1")));
    NetDeviceContainer staDevs1 = wifi1.Install(phy1, mac1, staGroup1);

    //NetDevice nos dispositivos ? nao eh pra por o 801154 aqui
    mac1.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("rede1")));
    NetDeviceContainer apDev1 = wifi1.Install(phy1, mac1, ap1);

    // Rede 2
    WifiHelper wifi2;
    wifi2.SetStandard(WIFI_STANDARD_80211n);
    WifiMacHelper mac2;
    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy2;
    phy2.SetChannel(channel2.Create());
    mac2.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("rede2")));
    NetDeviceContainer staDevs2 = wifi2.Install(phy2, mac2, staGroup2);

    //instalar 
    mac2.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("rede2")));
    NetDeviceContainer apDev2 = wifi2.Install(phy2, mac2, ap2);

    // Rede 3
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

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.InstallAll();

    Ipv6AddressHelper ip;

    ip.SetBase(Ipv6Address("2001:db8:0::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIfs1 = ip.Assign(staDevs1);
    Ipv6InterfaceContainer apIfs1 = ip.Assign(apDev1);

    ip.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIfs2 = ip.Assign(staDevs2);
    Ipv6InterfaceContainer apIfs2 = ip.Assign(apDev2);

    ip.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIfs3 = ip.Assign(staDevs3);
    Ipv6InterfaceContainer apIfs3 = ip.Assign(apDev3);

    // Links point-to-point entre APs
    ip.SetBase(Ipv6Address("fd00:0:0:0::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pIfs1 = ip.Assign(p2pDevs1);

    ip.SetBase(Ipv6Address("fd00:0:0:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pIfs2 = ip.Assign(p2pDevs2);

    ip.SetBase(Ipv6Address("fd00:0:0:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pIfs3 = ip.Assign(p2pDevs3);

    p2p.EnablePcapAll("p2p-trace");
    phy1.EnablePcap("wifi1", staDevs1.Get(0));
    phy3.EnablePcap("wifi3", staDevs3.Get(0));
    
    // Imprimir interfaces para debug
    std::cout << "AP1 Interfaces: " << std::endl;
    Ptr<Ipv6> Ipv6AP1 = ap1.Get(0)->GetObject<Ipv6>();
    for (uint32_t i = 0; i < Ipv6AP1->GetNInterfaces(); i++) {
        for(uint32_t j = 0; j < Ipv6AP1->GetNAddresses(i); j++){
            Ipv6Address addr = Ipv6AP1->GetAddress(i, j).GetAddress();
            // if(j == 0)
                std::cout << "  Interface " << i << "address" << j << ": " << addr << std::endl;
        }
    }

    std::cout << "AP2 Interfaces: " << std::endl;
    Ptr<Ipv6> Ipv6AP2 = ap2.Get(0)->GetObject<Ipv6>();
    for (uint32_t i = 0; i < Ipv6AP2->GetNInterfaces(); i++) {
        for(uint32_t j = 0; j < Ipv6AP2->GetNAddresses(i); j++){
            Ipv6Address addr = Ipv6AP2->GetAddress(i, j).GetAddress();
            // if(j == 0)
                std::cout << "  Interface: " << i << "address " <<  j << ": "<< addr << std::endl;
        }
    }

    std::cout << "AP3 Interfaces: " << std::endl;
    Ptr<Ipv6> Ipv6AP3 = ap3.Get(0)->GetObject<Ipv6>();
    for (uint32_t i = 0; i < Ipv6AP3->GetNInterfaces(); i++) {
        for(uint32_t j = 0; j < Ipv6AP3->GetNAddresses(i); j++){
            Ipv6Address addr = Ipv6AP3->GetAddress(i, j).GetAddress();
            // if(j == 0)
                std::cout << "  Interface: " << i << "address " <<  j << ": "<< addr << std::endl;
        }
    }

    // Create an UDP Echo server on n2
    uint32_t port = 9;
    uint32_t maxPacketCount = 5;
    UdpEchoServerHelper echoServer(port); // porta padrão para recebimento dos pacotes
    ApplicationContainer serverApps = echoServer.Install(ap1);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(30.0));

    // Create an UDP Echo client on n1 to send UDP packets to n2 via r1
    uint32_t packetSizeAP3 = 1600; // Packet should fragment as intermediate link MTU is 1500
    UdpEchoClientHelper echoClient(staIfs3.GetAddress(1, 1), port);
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSizeAP3));
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));

    ApplicationContainer clientAppsAP3 = echoClient.Install(staGroup3);
    clientAppsAP3.Start(Seconds(2.0));
    clientAppsAP3.Stop(Seconds(10.0));

    Ipv6StaticRoutingHelper routingHelper;
    
    Ptr<Ipv6StaticRouting> staticRoutingAp1 = routingHelper.GetStaticRouting(Ipv6AP1);
    Ptr<Ipv6StaticRouting> staticRoutingAp2 = routingHelper.GetStaticRouting(Ipv6AP2);
    Ptr<Ipv6StaticRouting> staticRoutingAp3 = routingHelper.GetStaticRouting(Ipv6AP3);
    
    // AP1 (interfaces são 0=lo, 1=wifi, 2=p2p_ap2, 3=p2p_ap3)
    //AP1 -> AP2
    staticRoutingAp1->AddNetworkRouteTo(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64), p2pIfs1.GetAddress(0,0), 2);
    //AP2 -> AP1
    staticRoutingAp2->AddNetworkRouteTo(Ipv6Address("2001:db8:0::"), Ipv6Prefix(64), p2pIfs1.GetAddress(1,0), 2);

    //AP1 -> AP3
    // staticRoutingAp1 = routingHelper.GetStaticRouting(Ipv6AP1);
    staticRoutingAp1->AddNetworkRouteTo(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64), p2pIfs3.GetAddress(0,0), 3);
    //AP3 -> AP1
    staticRoutingAp3->AddNetworkRouteTo(Ipv6Address("2001:db8:0::"), Ipv6Prefix(64), p2pIfs3.GetAddress(1,0), 3);

    //AP2 -> AP3
    staticRoutingAp2->AddNetworkRouteTo(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64), p2pIfs2.GetAddress(0,0), 2);
    //AP3 -> AP2
    staticRoutingAp3->AddNetworkRouteTo(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64), p2pIfs2.GetAddress(1,0), 2);    

    // // AP3 (interfaces são 0=lo, 1=wifi, 2=p2p_ap2, 3=p2p_ap1)
    // Ptr<Ipv6StaticRouting> staticRoutingAp3 = routingHelper.GetStaticRouting(Ipv6AP3);
    // staticRoutingAp3->AddNetworkRouteTo(Ipv6Address("fd00:2::"), Ipv6Prefix(64), p2pIfs3.GetAddress(1,0), 3);
    
    // Rotas default para as estações da rede 1
    for (uint32_t i = 0; i < staGroup1.GetN(); i++) {
        Ptr<Ipv6StaticRouting> staticRoutingSta = routingHelper.GetStaticRouting(staGroup1.Get(i)->GetObject<Ipv6>());
        staticRoutingSta->SetDefaultRoute(apIfs1.GetAddress(0,0), 1);
    }
    
    for (uint32_t i = 0; i < staGroup2.GetN(); i++) {
        Ptr<Ipv6StaticRouting> staticRoutingSta = routingHelper.GetStaticRouting(staGroup2.Get(i)->GetObject<Ipv6>());
        staticRoutingSta->SetDefaultRoute(apIfs2.GetAddress(0,0), 1);
    }

    for (uint32_t i = 0; i < staGroup3.GetN(); i++) {
        Ptr<Ipv6StaticRouting> staticRoutingSta = routingHelper.GetStaticRouting(staGroup3.Get(i)->GetObject<Ipv6>());
        staticRoutingSta->SetDefaultRoute(apIfs3.GetAddress(0,0), 1);
    }

    // // Rotas default para as estações da rede 3
    // for (uint32_t i = 0; i < staGroup3.GetN(); i++) {
    //     Ptr<Ipv6StaticRouting> staticRoutingSta = routingHelper.GetStaticRouting(staGroup3.Get(i)->GetObject<Ipv6>());
    //     staticRoutingSta->SetDefaultRoute(apIfs3.GetAddress(0,0), 1);
    // }
    
    // Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    // Imprimir tabela de roteamento para diagnóstico
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("routing_ipv6.txt", std::ios::out);
    routingHelper.PrintRoutingTableAt(Seconds(0.5), ap2.Get(0), routingStream);
    routingHelper.PrintRoutingTableAt(Seconds(0.5), ap3.Get(0), routingStream);

    // Ping de um nó da rede 1 para um nó da rede 3
    // PingHelper ping(staIfs2.GetAddress(1,0));
    // ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    // ping.SetAttribute("Size", UintegerValue(1024));
    // ping.SetAttribute("Count", UintegerValue(5));

    // ApplicationContainer pingApp = ping.Install(staGroup2.Get(0));
    // pingApp.Start(Seconds(30.0));
    // pingApp.Stop(Seconds(110.0));

    // Simulação
    Simulator::Stop(Seconds(120.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}