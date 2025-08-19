#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ping-helper.h"
#include "ns3/csma-module.h"
#include <ns3/lr-wpan-module.h>

using namespace ns3;

int main() {

    LogComponentEnable("Ping", LOG_LEVEL_INFO);
    
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

    NodeContainer ap1StaGroup1(staGroup1, ap1.Get(0));
    // NodeContainer ap1StaGroup2(staGroup2, ap2);
    // NodeContainer ap1StaGroup3(staGroup2, ap3);
    
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevs1 = p2p.Install(ap1ap2);
    NetDeviceContainer p2pDevs2 = p2p.Install(ap2ap3);
    NetDeviceContainer p2pDevs3 = p2p.Install(ap3ap1);

    InternetStackHelper internet;
    // internet.SetIpv6StackInstall(true);
    internet.SetIpv4StackInstall(false);
    internet.SetIpv6StackInstall(true);
    internet.Install(staGroup1);
    internet.Install(ap1);
    internet.Install(staGroup2);
    internet.Install(ap2);
    internet.Install(staGroup3);
    internet.Install(ap3);
    // internet.Install(ap1StaGroup1);

    // CsmaHelper csma;
    // csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    // csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    // NetDeviceContainer apGroup1 = csma.Install(ap1StaGroup1);
    // NetDeviceContainer apGroup2 = csma.Install(ap1StaGroup2);

    // LrWpanHelper lrWpan1;
    // NetDeviceContainer staDevs1 = lrWpan1.Install(staGroup1);
    // lrWpan1.CreateAssociatedPan(staDevs1, 10);    
    
    // LrWpanHelper lrWpan2;
    // NetDeviceContainer staDevs2 = lrWpan2.Install(staGroup2);
    // lrWpan2.CreateAssociatedPan(staDevs2, 10);    

    // LrWpanHelper lrWpan3;
    // NetDeviceContainer staDevs3 = lrWpan3.Install(staGroup3);
    // lrWpan3.CreateAssociatedPan(staDevs3, 10);    


    // Rede 1
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

    // Rede 2
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

    // NetDeviceContainer apLrWpanDev1 = lrWpan1.Install(ap1);
    // lrWpan1.CreateAssociatedPan(apLrWpanDev1, 10);

    // ip.SetBase(Ipv6Address("2001:DB8:0::"), Ipv6Prefix(64));
    // Ipv6InterfaceContainer apLrIfs1 = ip.Assign(apLrWpanDev1);


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
    ip.SetBase(Ipv6Address("2001:db8:a::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pIfs1 = ip.Assign(p2pDevs1);

    ip.SetBase(Ipv6Address("2001:db8:b::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pIfs2 = ip.Assign(p2pDevs2);

    ip.SetBase(Ipv6Address("2001:db8:c::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pIfs3 = ip.Assign(p2pDevs3);

    p2p.EnablePcapAll("p2p-trace");
    phy1.EnablePcap("wifi1", staDevs1.Get(0));
    phy3.EnablePcap("wifi3", staDevs3.Get(0));
    
    for (auto node : {ap1.Get(0), ap2.Get(0), ap3.Get(0)}) {
        Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
        for (uint32_t i = 0; i < ipv6->GetNInterfaces(); ++i) {
            ipv6->SetForwarding(i, true);
        }
    }

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

    Ipv6StaticRoutingHelper routingHelper;
    
    Ptr<Ipv6StaticRouting> staticRoutingAp1 = routingHelper.GetStaticRouting(Ipv6AP1);
    Ptr<Ipv6StaticRouting> staticRoutingAp2 = routingHelper.GetStaticRouting(Ipv6AP2);
    Ptr<Ipv6StaticRouting> staticRoutingAp3 = routingHelper.GetStaticRouting(Ipv6AP3);

    // AP1 (interfaces são 0=lo, 1=wifi, 2=p2p_ap2, 3=p2p_ap3)
    // AP1 -> AP2                               endereço da rede AP2 - staGroup2
    staticRoutingAp1->AddNetworkRouteTo(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64), 
                                        p2pIfs1.GetAddress(1,1), 2, 0);
    //AP1 -> AP3
    staticRoutingAp1->AddNetworkRouteTo(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64), 
                                        p2pIfs3.GetAddress(0,1), 3, 10);
                                    //  endereço 


    // staticRoutingAp1->AddNetworkRouteTo(Ipv6Address("2001:db8:0::"), Ipv6Prefix(64), 
    //                                     Ipv6Address::GetZero(), 1, 0);

    //AP2 -> AP1            
    staticRoutingAp2->AddNetworkRouteTo(Ipv6Address("2001:db8:0::"), Ipv6Prefix(64), 
                                        p2pIfs1.GetAddress(0,1), 1, 0);
    
    //AP2 -> AP3
    staticRoutingAp2->AddNetworkRouteTo(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64), 
                                        p2pIfs2.GetAddress(1,1), 2, 10);


    //AP3 -> AP1
    staticRoutingAp3->AddNetworkRouteTo(Ipv6Address("2001:db8:0::"), Ipv6Prefix(64), 
                                        p2pIfs3.GetAddress(1,1), 1, 10);
 
    //AP3 -> AP2
    staticRoutingAp3->AddNetworkRouteTo(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64), 
                                        p2pIfs2.GetAddress(0,1), 2, 10);    
    

    //p2pIfs1 -> p2pDevs1 -> ap1ap2
    //p2pIfs2 -> p2pDevs2 -> ap2ap3
    //p2pIfs3 -> p2pDevs3 -> ap3ap1
    
    // endereço ap1 --> 2001:db8:1:: 
    // for (uint32_t i = 0; i < staGroup1.GetN(); i++) {
        // }
        
    // Rotas default para as estações da rede 1
    Ptr<Ipv6> ap1_Ipv6 = ap1.Get(0)->GetObject<Ipv6>();
    // uint32_t ifAP1 = ap1_Ipv6->GetInterfaceForAddress(apIfs1.GetAddress(0,1));
    for (uint32_t i = 0; i < staGroup1.GetN(); i++) {
        Ptr<Ipv6StaticRouting> staticRoutingSta = routingHelper.GetStaticRouting(staGroup1.Get(i)->GetObject<Ipv6>());
        staticRoutingSta->SetDefaultRoute(apIfs1.GetAddress(0,1), 1);

        // Ipv6Address gateway = apIfs1.GetAddress(0,1);

        Ptr<Ipv6StaticRouting> sta1StaticRouting = routingHelper.GetStaticRouting(staGroup1.Get(1)->GetObject<Ipv6>());
        sta1StaticRouting->AddNetworkRouteTo(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64), 
                                            Ipv6Address("2001:db8:0::1"), 2, 0);

        // Ptr<Ipv6StaticRouting> ap2StaticRouting = routingHelper.GetStaticRouting(ap2.Get(0)->GetObject<Ipv6>());
        // ap2StaticRouting->AddNetworkRouteTo(Ipv6Address("2001:db8:0::"), Ipv6Prefix(64),
        //                                   p2pIfs1.GetAddress(0,1), 0);
    }
    
    for (uint32_t i = 0; i < staGroup2.GetN(); i++) {
        Ptr<Ipv6StaticRouting> staticRoutingSta = routingHelper.GetStaticRouting(staGroup2.Get(i)->GetObject<Ipv6>());
        staticRoutingSta->SetDefaultRoute(apIfs2.GetAddress(0,1), 1);
        // staticRoutingSta = routingHelper.AddNetworkRouteTo(Ipv6Address)
    }

    for (uint32_t i = 0; i < staGroup3.GetN(); i++) {
        Ptr<Ipv6StaticRouting> staticRoutingSta = routingHelper.GetStaticRouting(staGroup3.Get(i)->GetObject<Ipv6>());
        staticRoutingSta->SetDefaultRoute(apIfs3.GetAddress(0,1), 1);
    }
    
    
    Ptr<Ipv6StaticRouting> sta1StaticRouting = routingHelper.GetStaticRouting(staGroup1.Get(1)->GetObject<Ipv6>());
    

    // sta1StaticRouting->SetDefaultRoute(apIfs3.GetAddress(0,1), 1);

    // Imprimir tabela de roteamento para diagnóstico

    std::cout << "numero de endereços no nó staGroup1 : " << staGroup1.GetN() << std::endl;
    std::cout << "endereço ap1 :" << p2pIfs1.GetAddress(0,0) << std::endl;

    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("rotas_test_ping.txt", std::ios::out);
    routingHelper.PrintRoutingTableAt(Seconds(0.5), ap1.Get(0), routingStream);
    routingHelper.PrintRoutingTableAt(Seconds(0.6), ap2.Get(0), routingStream);
    routingHelper.PrintRoutingTableAt(Seconds(0.7), ap3.Get(0), routingStream);

   //p2pIfs1 -> p2pDevs1 -> ap1ap2
   //p2pIfs2 -> p2pDevs2 -> ap2ap3
   //p2pIfs3 -> p2pDevs3 -> ap3ap1

    // Ping de um nó da rede 1 para um nó da rede 3
    PingHelper ping(apIfs2.GetAddress(0, 1));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(512));
    ping.SetAttribute("Count", UintegerValue(10));

    // Ptr<Node> apNode = ap1.Get(0);
    ApplicationContainer pingApp = ping.Install(staGroup1.Get(1));
    pingApp.Start(Seconds(30.0));
    pingApp.Stop(Seconds(110.0));

    // Simulação
    Simulator::Stop(Seconds(120.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
