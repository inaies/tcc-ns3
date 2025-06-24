// Network topology
//  - 1 AP fixed at (100,100) and 20 nodes on a 200 x 200 grid from (0,0) to (200,200)
//  - All the nodes are evenly spaced
//  - The nodes' range is 50m wide, while the AP's is 200m
//  - The nodes should ping the AP then Multicast ping all the other nodes within its PAN
//  - At first, let's make only a few nodes ping: 
//      + One at the edge of the network    -> Node 0   (141 m from AP)
//      + One really close to the AP        -> Node 12  (14.1 m from AP)
//      + One in the "middle"               -> Node 8    ()
//  - After that, let's make the AP Multicast ping all of the nodes and view that effect in the 3 nodes above
//  - There will only be 4 pcaps, 1 for each node and 1 for the AP

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"

#include "ns3/node-container.h"
#include "ns3/lr-wpan-helper.h"
#include "ns3/lr-wpan-net-device.h"
#include "ns3/lr-wpan-spectrum-value-helper.h"
#include "ns3/spectrum-value.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/sixlowpan-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/mobility-module.h"
#include "sys/stat.h"
#include <iostream>
#include <fstream>

#include "ns3/netanim-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("WnetIIOTv1");

int main (int argc, char *argv[]){
    uint16_t numNodes = 20;
    std::string animFile = "wnetIIOTv1-animation.xml";

    LogComponentEnable ("WnetIIOTv1", LOG_LEVEL_INFO);

    NS_LOG_INFO ("Create nodes.");
    NodeContainer nodes, apNode; 
    nodes.Create(numNodes);
    apNode.Create(1); 
    NodeContainer allNodes(nodes, apNode); //apNode == allNodes[numNodes]

    NS_LOG_INFO("Set up mobility.");
    double distNodes = ceil(sqrt(40000/numNodes)); 
	double nodesPerLine = ceil(200/distNodes);

    MobilityHelper nodesMobility;
    nodesMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    nodesMobility.SetPositionAllocator("ns3::GridPositionAllocator",    
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(distNodes),
        "DeltaY", DoubleValue(distNodes),
        "GridWidth", UintegerValue(nodesPerLine),
        "LayoutType", StringValue("RowFirst")
    );
    nodesMobility.Install(nodes);

    double apPosX = 100, apPosY = 100;
    MobilityHelper apMobilityHelper;
    apMobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	apMobilityHelper.Install(apNode);
    Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
	apMobility->SetPosition(Vector(apPosX, apPosY, 0.0));

    NS_LOG_INFO("Set up and install LR-WPAN");
    LrWpanHelper lrwpan;
    NetDeviceContainer wpanDevices = lrwpan.Install(allNodes);
    lrwpan.AssociateToPan(wpanDevices, 0); //associate nodes to the same PAN (id=0)

    NS_LOG_INFO("Set up and install 6LoWPAN");
    SixLowPanHelper sixlowpan; //6lowpan allows LR-WPAN to use IPv6
    NetDeviceContainer netDevices = sixlowpan.Install(wpanDevices);

    NS_LOG_INFO ("Set devices ranges.");
    LrWpanSpectrumValueHelper svh200m;
    Ptr<SpectrumValue> psd200m = svh200m.CreateTxPowerSpectralDensity (9, 11); //Range of 200m according to lr-wpan-error-distance-plot
    LrWpanSpectrumValueHelper svh50m;
    Ptr<SpectrumValue> psd50m = svh50m.CreateTxPowerSpectralDensity (-10, 11); //Range of 50m according to lr-wpan-error-distance-plot

    Ptr<LrWpanNetDevice> apNetDevice = DynamicCast<LrWpanNetDevice>(apNode.Get(0)->GetDevice(0));
    auto apPhy = apNetDevice->GetPhy();
    apPhy->SetTxPowerSpectralDensity(psd200m);

    for(uint16_t i = 0; i < numNodes; ++i){
        Ptr<LrWpanNetDevice> nodeNetDevice = DynamicCast<LrWpanNetDevice>(nodes.Get(i)->GetDevice(0));
        auto nodePhy = nodeNetDevice->GetPhy();
        apPhy->SetTxPowerSpectralDensity(psd50m);
    }

    NS_LOG_INFO("Install internet protocols stack.");
    InternetStackHelper internet;
    internet.SetIpv4StackInstall(false);
	internet.SetIpv6StackInstall(true);
	internet.Install(allNodes);

    NS_LOG_INFO("Create and install IPv6 adresses.");
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  	Ipv6InterfaceContainer subnet1 = ipv6.Assign(netDevices);  //subnet1 global address -> 2001:1:0:0:x:x:x:x

    NS_LOG_INFO("Setup and install ping6 applications.");
    uint32_t packetSize = 19;
    uint32_t maxPacketCount = 1;
    Time interPacketInterval = Seconds (1.);

    uint32_t edgeNode = 0;
    uint32_t midNode = 8;
    uint32_t innerNode = 12;

    auto apAddress = apNode.Get(0)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress();               //ap link-layer address
    auto edgeNodeAddress = nodes.Get(edgeNode)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress();   //edgeNode link-layer address
    auto midNodeAddress = nodes.Get(midNode)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress();     //midNode link-layer address
    auto innerNodeAddress = nodes.Get(innerNode)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress(); //innerNode link-layer address

    //ping edgeNode -> Multicast (FF02::1)
    Ping6Helper pingEdge2MultHelper;
    pingEdge2MultHelper.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    pingEdge2MultHelper.SetAttribute ("Interval", TimeValue (interPacketInterval));
    pingEdge2MultHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
    pingEdge2MultHelper.SetLocal(edgeNodeAddress); 
    pingEdge2MultHelper.SetRemote(Ipv6Address::GetAllNodesMulticast ()); 

    ApplicationContainer pingEdge2Mult = pingEdge2MultHelper.Install(nodes.Get(edgeNode)); 
    pingEdge2Mult.Start(Seconds(4.0));
    pingEdge2Mult.Stop(Seconds(5.5));

    //ping MidNode -> Multicast (FF02::1)
    Ping6Helper pingMid2MultHelper;
    pingMid2MultHelper.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    pingMid2MultHelper.SetAttribute ("Interval", TimeValue (interPacketInterval));
    pingMid2MultHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
    pingMid2MultHelper.SetLocal(midNodeAddress); 
    pingMid2MultHelper.SetRemote(Ipv6Address::GetAllNodesMulticast ()); 

    ApplicationContainer pingMid2Mult = pingMid2MultHelper.Install(nodes.Get(midNode)); 
    pingMid2Mult.Start(Seconds(8.0));
    pingMid2Mult.Stop(Seconds(9.5));

    //ping InnerNode -> Multicast (FF02::1)
    Ping6Helper pingInner2MultHelper;
    pingInner2MultHelper.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    pingInner2MultHelper.SetAttribute ("Interval", TimeValue (interPacketInterval));
    pingInner2MultHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
    pingInner2MultHelper.SetLocal(innerNodeAddress); 
    pingInner2MultHelper.SetRemote(Ipv6Address::GetAllNodesMulticast ()); 

    ApplicationContainer pingInner2Mult = pingInner2MultHelper.Install(nodes.Get(innerNode)); 
    pingInner2Mult.Start(Seconds(12.0));
    pingInner2Mult.Stop(Seconds(13.5));

    //ping AP -> Multicast (FF02::1)
    Ping6Helper pingAP2MultHelper;
    pingAP2MultHelper.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    pingAP2MultHelper.SetAttribute ("Interval", TimeValue (interPacketInterval));
    pingAP2MultHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
    pingAP2MultHelper.SetLocal(apAddress); 
    pingAP2MultHelper.SetRemote(Ipv6Address::GetAllNodesMulticast ()); 

    ApplicationContainer pingAP2Mult = pingAP2MultHelper.Install(apNode.Get(0)); 
    pingAP2Mult.Start(Seconds(15.0));
    pingAP2Mult.Stop(Seconds(16.5));

    NS_LOG_INFO("Setup tracing");
    AsciiTraceHelper ascii;
    lrwpan.EnableAsciiAll(ascii.CreateFileStream ("wnet-IIOT-v1.tr"));
    lrwpan.EnablePcapAll(std::string ("wnet-IIOT-v1"), true);

    NS_LOG_INFO("Setup animation");
    AnimationInterface anim (animFile);

    NS_LOG_INFO("Setup Simulator");
    Simulator::Stop (Seconds (20.0));
  
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}