// ... (mesmos includes)

// Troque o bloco de canal/phy único por DOIS canais/phys:
YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
YansWifiPhyHelper     phy1;
phy1.SetChannel(channel1.Create());

YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
YansWifiPhyHelper     phy2;
phy2.SetChannel(channel2.Create());

WifiMacHelper mac;
Ssid ssid1 = Ssid("ns-3-ssid-1");
Ssid ssid2 = Ssid("ns-3-ssid-2"); // opcional, mas ajuda

WifiHelper wifi;

// --- Primeira rede Wi-Fi (wifiStaNodes <-> wifiApNode) ---
NetDeviceContainer staDevices;
mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
staDevices = wifi.Install(phy1, mac, wifiStaNodes);

NetDeviceContainer apDevices;
mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
apDevices = wifi.Install(phy1, mac, wifiApNode);

// --- Segunda rede Wi-Fi (wifiStaNodes2 <-> wifiApNode2) ---
NetDeviceContainer staDevices2;
mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);

NetDeviceContainer apDevices2;
mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

// ... (mesma mobilidade, pilha IP, etc.)

Ipv4AddressHelper address;

// p2p e CSMA como estavam
address.SetBase("10.1.1.0", "255.255.255.0");
Ipv4InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

address.SetBase("10.1.2.0", "255.255.255.0");
Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

// *** CORRIGIR AQUI: sub-rede válida para a 1ª rede Wi-Fi ***
address.SetBase("10.1.4.0", "255.255.255.0");
Ipv4InterfaceContainer wifiInterfaces = address.Assign(staDevices);
Ipv4InterfaceContainer apInterfaces   = address.Assign(apDevices);

// 2ª rede Wi-Fi fica em outra sub-rede (já estava ok)
address.SetBase("10.1.3.0", "255.255.255.0");
Ipv4InterfaceContainer wifiInterfaces2 = address.Assign(staDevices2);
Ipv4InterfaceContainer apInterfaces2   = address.Assign(apDevices2);

// p2p restantes como estavam
address.SetBase("10.1.11.0", "255.255.255.0");
Ipv4InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

address.SetBase("10.1.12.0", "255.255.255.0");
Ipv4InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);

//
