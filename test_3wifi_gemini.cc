#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h" // Mantido por compatibilidade
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

// UTIL: retorna o interface index do Ipv6 que corresponde a um NetDevice em um Node.
// Retorna -1 se não encontrado.
static int32_t
GetIpv6InterfaceIndexForDevice(Ptr<Ipv6> ipv6, Ptr<NetDevice> nd)
{
    if (ipv6 == nullptr || nd == nullptr)
    {
        return -1;
    }
    uint32_t nIf = ipv6->GetNInterfaces();
    for (uint32_t ifIndex = 0; ifIndex < nIf; ++ifIndex)
    {
        Ptr<NetDevice> dev = ipv6->GetNetDevice(ifIndex);
        if (dev == nd)
        {
            return static_cast<int32_t>(ifIndex);
        }
    }
    return -1;
}

int main(int argc, char* argv[])
{
    LogComponentEnable("Ping", LOG_LEVEL_INFO);
    LogComponentEnable("ThirdScriptExample", LOG_LEVEL_INFO);

    bool verbose = true;
    uint32_t nWifiCsma = 120;
    uint32_t nWifi = 120;
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifiCsma", "Number of STA devices in the new WiFi 3 network", nWifiCsma);
    cmd.AddValue("nWifi", "Number of STA devices in WiFi 1 and 2", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    // Ativar RAs IPv6 (muito útil para autoconfiguração de endereços)
    Config::SetDefault("ns3::Ipv6L3Protocol::EnableRouterAdvertisements", BooleanValue(true));

    NodeContainer p2pNodes;
    p2pNodes.Create(3); // n0=AP1, n1=AP2/WiFi3 AP, n2=AP3

    // Ponto-a-ponto
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(1));
    NetDeviceContainer ap1ap3 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));
    NetDeviceContainer ap2ap3 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));

    // WiFis
    NodeContainer wifiStaNodes1, wifiStaNodes2, wifiStaNodes3;
    wifiStaNodes1.Create(nWifi);
    wifiStaNodes2.Create(nWifi);
    wifiStaNodes3.Create(nWifiCsma);

    NodeContainer wifiApNode = NodeContainer(p2pNodes.Get(0)); // AP1 (n0)
    NodeContainer wifiApNode2 = NodeContainer(p2pNodes.Get(2)); // AP3 (n2)
    NodeContainer wifiApNode3 = NodeContainer(p2pNodes.Get(1)); // AP2 (n1)

    // PHY / channel
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();

    YansWifiPhyHelper phy1 = YansWifiPhyHelper::Default();
    YansWifiPhyHelper phy2 = YansWifiPhyHelper::Default();
    YansWifiPhyHelper phy3 = YansWifiPhyHelper::Default();

    phy1.SetChannel(channel1.Create());
    phy2.SetChannel(channel2.Create());
    phy3.SetChannel(channel3.Create());

    // aumentar Tx e Rx para alcance (ajuste se quiser)
    phy1.Set("TxPowerStart", DoubleValue(20.0));
    phy1.Set("TxPowerEnd", DoubleValue(20.0));
    phy1.Set("RxGain", DoubleValue(0.0));
    phy2.Set("TxPowerStart", DoubleValue(20.0));
    phy2.Set("TxPowerEnd", DoubleValue(20.0));
    phy2.Set("RxGain", DoubleValue(0.0));
    phy3.Set("TxPowerStart", DoubleValue(20.0));
    phy3.Set("TxPowerEnd", DoubleValue(20.0));
    phy3.Set("RxGain", DoubleValue(0.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");
    Ssid ssid3 = Ssid("ns-3-ssid-3");

    // WiFi 1
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy1, mac, wifiStaNodes1);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifi.Install(phy1, mac, wifiApNode);

    // WiFi 2
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    // WiFi 3
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    NetDeviceContainer apDevices3 = wifi.Install(phy3, mac, wifiApNode3);

    // Mobilidade: STAs em grid espalhado
    MobilityHelper mobilityStas;
    mobilityStas.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(8.0),
        "DeltaY", DoubleValue(8.0),
        "GridWidth", UintegerValue(20),
        "LayoutType", StringValue("RowFirst"));
    mobilityStas.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(-400, 400, -400, 400)));
    mobilityStas.Install(wifiStaNodes1);
    mobilityStas.Install(wifiStaNodes2);
    mobilityStas.Install(wifiStaNodes3);

    // APs fixos (posições separadas)
    Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
    apPos->Add(Vector(0.0, 0.0, 0.0));     // AP1 (n0)
    apPos->Add(Vector(400.0, 0.0, 0.0));   // AP2 (n1)
    apPos->Add(Vector(200.0, 400.0, 0.0)); // AP3 (n2)
    MobilityHelper mobilityAps;
    mobilityAps.SetPositionAllocator(apPos);
    mobilityAps.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAps.Install(p2pNodes);

    // Roteamento / pilhas
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

    // Endereçamento IPv6 (atenção: ordem de Assign corresponde à ordem dos NetDevices)
    Ipv6AddressHelper address;
    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer wifiInterfaces1 = address.Assign(staDevices1);
    Ipv6InterfaceContainer apInterfaces1 = address.Assign(apDevices1);

    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer wifiInterfaces2 = address.Assign(staDevices2);
    Ipv6InterfaceContainer apInterfaces2 = address.Assign(apDevices2);

    address.SetBase(Ipv6Address("2001:7::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer wifiInterfaces3 = address.Assign(staDevices3);
    Ipv6InterfaceContainer apInterfaces3 = address.Assign(apDevices3);

    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);

    // Habilitar forwarding
    for (uint32_t i = 0; i < p2pNodes.GetN(); ++i)
    {
        Ptr<Ipv6> ipv6 = p2pNodes.Get(i)->GetObject<Ipv6>();
        if (ipv6) ipv6->SetForwarding(0, true);
    }

    // --- Rotas estáticas dos STAs (definir default route para o AP correspondente),
    // fazendo isso de forma segura: procuramos o interface index correspondente ao NetDevice.
    // Para os STAs, usamos os NetDevice containers (staDevices1,2,3) que têm mesma ordem que wifiStaNodes.

    // AP addresses (pegamos o primeiro endereço do apDevices container de cada AP)
    // Segurança: verificamos se apInterfacesX tem pelo menos um elemento
    Ipv6Address ap1Addr = Ipv6Address::GetZero();
    Ipv6Address ap2Addr = Ipv6Address::GetZero();
    Ipv6Address ap3Addr = Ipv6Address::GetZero();
    if (apInterfaces1.GetN() > 0) ap1Addr = apInterfaces1.GetAddress(0,0);
    if (apInterfaces2.GetN() > 0) ap3Addr = apInterfaces2.GetAddress(0,0);
    if (apInterfaces3.GetN() > 0) ap2Addr = apInterfaces3.GetAddress(0,0);

    // Função lambda para configurar default route de forma segura
    auto SetDefaultRoutesForStaNodes = [&](NodeContainer &staNodes, NetDeviceContainer &staDevices, Ipv6Address apAddr) {
        for (uint32_t i = 0; i < staNodes.GetN(); ++i)
        {
            Ptr<Node> node = staNodes.Get(i);
            Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
            if (!ipv6)
            {
                NS_LOG_WARN("Node " << node->GetId() << " sem Ipv6 (ignorando)");
                continue;
            }
            Ptr<NetDevice> nd = staDevices.Get(i);
            // verificar se o NetDevice pertence realmente ao Node
            if (nd->GetNode() != node)
            {
                NS_LOG_WARN("NetDevice index " << i << " não pertence ao node " << node->GetId() << " (pulando)");
                // tentamos localizar um NetDevice do node que seja do tipo WifiNetDevice (fallback)
                bool found = false;
                for (uint32_t d = 0; d < node->GetNDevices(); ++d)
                {
                    Ptr<NetDevice> cand = node->GetDevice(d);
                    if (cand->GetInstanceTypeId() == nd->GetInstanceTypeId())
                    {
                        nd = cand;
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    NS_LOG_WARN("Não foi possível localizar NetDevice compatível no node " << node->GetId());
                    continue;
                }
            }
            int32_t ifIndex = GetIpv6InterfaceIndexForDevice(ipv6, nd);
            if (ifIndex < 0)
            {
                NS_LOG_WARN("Não encontrou interface IPv6 para NetDevice no node " << node->GetId());
                continue;
            }
            Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
            sr->SetDefaultRoute(apAddr, (uint32_t)ifIndex);
        }
    };

    SetDefaultRoutesForStaNodes(wifiStaNodes1, staDevices1, ap1Addr);
    SetDefaultRoutesForStaNodes(wifiStaNodes2, staDevices2, ap3Addr);
    SetDefaultRoutesForStaNodes(wifiStaNodes3, staDevices3, ap2Addr);

    // --- Debug: listar alguns endereços de STAs (após associação/autoconfig)
    Simulator::Schedule(Seconds(8.0), [&](){
        NS_LOG_INFO("=== Debug: listando alguns endereços IPv6 dos STAs (se houver) ===");
        uint32_t check = std::min<uint32_t>(5, wifiStaNodes1.GetN());
        for (uint32_t i = 0; i < check; ++i)
        {
            Ptr<Node> n = wifiStaNodes1.Get(i);
            Ptr<Ipv6> ipv6 = n->GetObject<Ipv6>();
            if (!ipv6) { NS_LOG_INFO("Node " << n->GetId() << " sem Ipv6"); continue; }
            uint32_t nIf = ipv6->GetNInterfaces();
            for (uint32_t ifidx = 0; ifidx < nIf; ++ifidx)
            {
                uint32_t nAddr = ipv6->GetNAddresses(ifidx);
                for (uint32_t a = 0; a < nAddr; ++a)
                {
                    Ipv6InterfaceAddress ifAddr = ipv6->GetAddress(ifidx, a);
                    NS_LOG_INFO("Node " << n->GetId() << " if=" << ifidx << " addr=" << ifAddr.GetAddress());
                }
            }
        }
    });

    // --- Ping: exemplo — de um STA da WiFi1 para o primeiro STA da WiFi2 (se existir)
    //Selecionar endereços de destino de forma segura:
    Ipv6Address pingDestination = Ipv6Address::GetZero();
    if (wifiInterfaces2.GetN() > 0)
    {
        // wifiInterfaces2 foi retornado na ordem dos staDevices2; pegamos a primeira entrada se existir
        pingDestination = wifiInterfaces2.GetAddress(0,0);
    }
    if (pingDestination == Ipv6Address::GetZero())
    {
        NS_LOG_WARN("Não foi possível determinar destino de ping (wifiInterfaces2 vazio). Ping não será instalado.");
    }
    else
    {
        PingHelper ping(pingDestination);
        ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        ping.SetAttribute("Size", UintegerValue(512));
        ping.SetAttribute("Count", UintegerValue(10));
        // Fonte: um STA da WiFi1 (apenas se existir)
        if (wifiStaNodes1.GetN() > 2)
        {
            ApplicationContainer pingApp = ping.Install(wifiStaNodes1.Get(2));
            pingApp.Start(Seconds(30.0));
            pingApp.Stop(Seconds(110.0));
        }
        else
        {
            NS_LOG_WARN("Poucos STAs na WiFi1; ping não instalado.");
        }
    }

    Simulator::Stop(Seconds(120.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

