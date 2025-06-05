#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"
// #include "packet.h"

using namespace ns3;

int main() {

    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer staGroup1, ap1, staGroup2, ap2, staGroup3, ap3;
    staGroup1.Create(20);
    ap1.Create(1);
    staGroup2.Create(20);
    ap2.Create(1);
    staGroup3.Create(20);
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
    internet.Install(staGroup1);
    internet.Install(ap1);
    internet.Install(staGroup2);
    internet.Install(ap2);
    internet.Install(staGroup3);
    internet.Install(ap3);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    WifiMacHelper mac;
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();

    YansWifiPhyHelper phy1;
    phy1.SetChannel(channel1.Create());
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("rede1")));
    NetDeviceContainer staDevs1 = wifi.Install(phy1, mac, staGroup1);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("rede1")));
    NetDeviceContainer apDev1 = wifi.Install(phy1, mac, ap1);

    YansWifiPhyHelper phy2;
    phy2.SetChannel(channel2.Create());
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("rede2")));
    NetDeviceContainer staDevs2 = wifi.Install(phy2, mac, staGroup2);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("rede2")));
    NetDeviceContainer apDev2 = wifi.Install(phy2, mac, ap2);

    YansWifiPhyHelper phy3;
    phy3.SetChannel(channel3.Create());
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("rede3")));
    NetDeviceContainer staDevs3 = wifi.Install(phy3, mac, staGroup3);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("rede3")));
    NetDeviceContainer apDev3 = wifi.Install(phy3, mac, ap3);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.InstallAll();

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIfs1 = ipv6.Assign(staDevs1);
    Ipv6InterfaceContainer apIfs1 = ipv6.Assign(apDev1);

    ipv6.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIfs2 = ipv6.Assign(staDevs2);
    Ipv6InterfaceContainer apIfs2 = ipv6.Assign(apDev2);

    ipv6.SetBase(Ipv6Address("2001:db8:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer staIfs3 = ipv6.Assign(staDevs3);
    Ipv6InterfaceContainer apIfs3 = ipv6.Assign(apDev3);

    ipv6.SetBase(Ipv6Address("2001:db8:a::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pIfs1 = ipv6.Assign(p2pDevs1);
    ipv6.SetBase(Ipv6Address("2001:db8:b::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pIfs2 = ipv6.Assign(p2pDevs2);
    ipv6.SetBase(Ipv6Address("2001:db8:c::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pIfs3 = ipv6.Assign(p2pDevs3);

    for (auto node : {ap1.Get(0), ap2.Get(0), ap3.Get(0)}) {
        Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
        for (uint32_t i = 0; i < ipv6->GetNInterfaces(); ++i) {
            ipv6->SetForwarding(i, true);
        }
    }

    Ipv6StaticRoutingHelper routingHelper;

    // AP1 → rede 3 (via AP3)
    Ptr<Ipv6StaticRouting> staticRoutingAp1 = routingHelper.GetStaticRouting(ap1.Get(0)->GetObject<Ipv6>());
    staticRoutingAp1->AddNetworkRouteTo(Ipv6Address("2001:db8:3::"), Ipv6Prefix(64),
                                        p2pIfs3.GetAddress(0, 1), 3, 10);
    // Rota alternativa via AP2
    staticRoutingAp1->AddNetworkRouteTo(Ipv6Address("2001:db8:3::"), Ipv6Prefix(64),
                                        p2pIfs1.GetAddress(0, 1), 2, 20);

    // AP2 rotas
    Ptr<Ipv6StaticRouting> staticRoutingAp2 = routingHelper.GetStaticRouting(ap2.Get(0)->GetObject<Ipv6>());
    staticRoutingAp2->AddNetworkRouteTo(Ipv6Address("2001:db8:3::"), Ipv6Prefix(64),
                                        p2pIfs2.GetAddress(0, 1), 2, 10);
    staticRoutingAp2->AddNetworkRouteTo(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64),
                                        p2pIfs1.GetAddress(1, 1), 1, 10);

    // AP3 → rede 1
    Ptr<Ipv6StaticRouting> staticRoutingAp3 = routingHelper.GetStaticRouting(ap3.Get(0)->GetObject<Ipv6>());
    staticRoutingAp3->AddNetworkRouteTo(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64),
                                        p2pIfs3.GetAddress(1, 1), 3, 10);


    Ptr<Ipv6StaticRouting> staticRoutingAp2to3 = routingHelper.GetStaticRouting(ap2.Get(0)->GetObject<Ipv6>());
    staticRoutingAp1->AddNetworkRouteTo(Ipv6Address("2001:db8:3::"), Ipv6Prefix(64),
                                        p2pIfs2.GetAddress(0, 1), 3, 10);

    // Rota default para staGroup1
    Ptr<Ipv6> apIpv6_1 = ap1.Get(0)->GetObject<Ipv6>();
    uint32_t ifAp1 = apIpv6_1->GetInterfaceForAddress(apIfs1.GetAddress(0, 1));
    for (uint32_t i = 0; i < staGroup1.GetN(); i++) {
        Ptr<Ipv6StaticRouting> staticRoutingSta = routingHelper.GetStaticRouting(staGroup1.Get(i)->GetObject<Ipv6>());
        staticRoutingSta->SetDefaultRoute(apIfs1.GetAddress(0, 1), ifAp1);
    }

    // Rota default para staGroup3
    Ptr<Ipv6> apIpv6_3 = ap3.Get(0)->GetObject<Ipv6>();
    uint32_t ifAp3 = apIpv6_3->GetInterfaceForAddress(apIfs3.GetAddress(0, 1));
    for (uint32_t i = 0; i < staGroup3.GetN(); i++) {
        Ptr<Ipv6StaticRouting> staticRoutingSta = routingHelper.GetStaticRouting(staGroup3.Get(i)->GetObject<Ipv6>());
        staticRoutingSta->SetDefaultRoute(apIfs3.GetAddress(0, 1), ifAp3);
    }

    // Instalar servidor UDP Echo na estação da rede 3
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(staGroup3.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(50.0));

    std::cout << "Servidor UDP IPv6: " << staIfs3.GetAddress(0, 1) << std::endl;

    // Instalar cliente na estação da rede 1
    // UdpEchoClientHelper echoClient(staIfs3.GetAddress(0, 1), 9);
    UdpEchoClientHelper echoClient(staIfs2.GetAddress(0, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    // ApplicationContainer clientApps = echoClient.Install(staGroup1.Get(0));
    ApplicationContainer clientApps = echoClient.Install(ap1.Get(0));
    clientApps.Start(Seconds(10.0));
    clientApps.Stop(Seconds(50.0));

    Simulator::Stop(Seconds(60.0));
    AnimationInterface anim("ipv6.xml");
    anim.SetConstantPosition(ap1.Get(0), 10, 10);
    anim.SetConstantPosition(ap2.Get(0), 20, 10);    
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
