# -*- coding: utf-8 -*-
from ns import ns
import sys
import ns.applications
import ns.core
import ns.csma
import ns.internet
import ns.mobility
import ns.network
import ns.point_to_point
import ns.wifi
import ns.ripng
import math

from ns.core import LogLevel as LL
from ns.network import Ipv6Address, Ipv6Prefix, Ipv6InterfaceContainer, Ipv6StaticRouting
from ns.mobility import Vector
from ns.wifi import Ssid, WifiPhyHelper

# Definição da função para alocar posições em grade
def create_grid_position_allocator(n_nodes, spacing, offset_x, offset_y):
    """
    Cria e retorna um ListPositionAllocator para dispor nós em uma grade.
    """
    allocator = ns.mobility.ListPositionAllocator()
    if n_nodes == 0:
        return allocator
    
    # Equivalente a std::ceil(std::sqrt(nNodes))
    cols = int(math.ceil(math.sqrt(n_nodes)))
    
    for i in range(n_nodes):
        row = i // cols
        col = i % cols
        x = offset_x + col * spacing
        y = offset_y + row * spacing
        allocator.Add(Vector(x, y, 0.0))
        
    return allocator

# Configuração de logging
ns.core.LogComponentEnable("Ping", LL.INFO)
ns.core.LogComponentEnable("ThirdScriptExample", LL.INFO)
ns.core.LogComponentEnable("UdpEchoClientApplication", LL.INFO)
ns.core.LogComponentEnable("UdpEchoServerApplication", LL.INFO)
ns.core.LogComponentEnable("OnOffApplication", LL.INFO)
ns.core.LogComponentEnable("PacketSink", LL.INFO)


def main(argv):
    # Valores default
    verbose = True
    n_wifi_sta_3 = 173  # nCsma renomeado
    n_wifi_sta_1_2 = 173
    tracing = True

    # Processamento da linha de comando
    cmd = ns.core.CommandLine()
    cmd.AddValue("nWifiCsma", "Number of STA devices in the new WiFi 3 network", n_wifi_sta_3)
    cmd.AddValue("nWifi", "Number of STA devices in WiFi 1 and 2", n_wifi_sta_1_2)
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose)
    cmd.AddValue("tracing", "Enable pcap tracing", tracing)
    cmd.Parse(argv)

    if n_wifi_sta_1_2 > 200:
        print("nWifi muito grande; ajuste o script ou aumente a área.")
        return 1

    # Criação de nós AP (Roteadores)
    p2p_nodes = ns.network.NodeContainer()
    p2p_nodes.Create(3)  # n0=AP1, n1=AP2, n2=AP3 (WiFi3 AP no C++ original)

    # Configuração Ponto-a-Ponto
    point_to_point = ns.point_to_point.PointToPointHelper()
    point_to_point.SetDeviceAttribute("DataRate", ns.core.StringValue("5Mbps"))
    point_to_point.SetChannelAttribute("Delay", ns.core.StringValue("2ms"))

    ap1_ap2 = point_to_point.Install(p2p_nodes.Get(0), p2p_nodes.Get(1))
    ap1_ap3 = point_to_point.Install(p2p_nodes.Get(0), p2p_nodes.Get(2))
    ap2_ap3 = point_to_point.Install(p2p_nodes.Get(1), p2p_nodes.Get(2))

    # --- WiFi nodes ---
    wifi_sta_nodes_1 = ns.network.NodeContainer()
    wifi_sta_nodes_1.Create(n_wifi_sta_1_2)
    wifi_sta_nodes_2 = ns.network.NodeContainer()
    wifi_sta_nodes_2.Create(n_wifi_sta_1_2)
    wifi_sta_nodes_3 = ns.network.NodeContainer()
    wifi_sta_nodes_3.Create(n_wifi_sta_3)

    wifi_ap_node_1 = p2p_nodes.Get(0)  # AP1
    wifi_ap_node_2 = p2p_nodes.Get(1)  # AP2
    wifi_ap_node_3 = p2p_nodes.Get(2)  # AP3

    # PHY/MAC
    # WiFi 1 (AP1)
    channel1 = ns.wifi.YansWifiChannelHelper.Default()
    phy1 = ns.wifi.YansWifiPhyHelper()
    phy1.SetChannel(channel1.Create())
    phy1.Set("ChannelSettings", ns.core.StringValue("{36, 0, BAND_5GHZ, 0}"))

    # WiFi 2 (AP2)
    channel2 = ns.wifi.YansWifiChannelHelper.Default()
    phy2 = ns.wifi.YansWifiPhyHelper()
    phy2.SetChannel(channel2.Create())
    phy2.Set("ChannelSettings", ns.core.StringValue("{40, 0, BAND_5GHZ, 0}"))

    # WiFi 3 (AP3)
    channel3 = ns.wifi.YansWifiChannelHelper.Default()
    phy3 = ns.wifi.YansWifiPhyHelper()
    phy3.SetChannel(channel3.Create())
    phy3.Set("ChannelSettings", ns.core.StringValue("{44, 0, BAND_5GHZ, 0}"))

    mac = ns.wifi.WifiMacHelper()
    wifi = ns.wifi.WifiHelper()
    wifi.SetStandard(ns.wifi.WIFI_STANDARD_80211n)

    ssid1 = Ssid("ns-3-ssid-1")
    ssid2 = Ssid("ns-3-ssid-2")
    ssid3 = Ssid("ns-3-ssid-3")

    # WiFi 1 (AP1) - STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", ns.core.SsidValue(ssid1),
                "ActiveProbing", ns.core.BooleanValue(False))
    sta_devices_1 = wifi.Install(phy1, mac, wifi_sta_nodes_1)
    # WiFi 1 (AP1) - AP
    mac.SetType("ns3::ApWifiMac", "Ssid", ns.core.SsidValue(ssid1))
    ap_devices_1 = wifi.Install(phy1, mac, ns.network.NodeContainer(wifi_ap_node_1))
    # No C++ original, wifiApNode é um NodeContainer de 1 nó; em Python, precisamos recriar

    # WiFi 2 (AP2) - STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", ns.core.SsidValue(ssid2),
                "ActiveProbing", ns.core.BooleanValue(False))
    sta_devices_2 = wifi.Install(phy2, mac, wifi_sta_nodes_2)
    # WiFi 2 (AP2) - AP
    mac.SetType("ns3::ApWifiMac", "Ssid", ns.core.SsidValue(ssid2))
    ap_devices_2 = wifi.Install(phy2, mac, ns.network.NodeContainer(wifi_ap_node_2))

    # WiFi 3 (AP3) - STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", ns.core.SsidValue(ssid3),
                "ActiveProbing", ns.core.BooleanValue(False))
    sta_devices_3 = wifi.Install(phy3, mac, wifi_sta_nodes_3)
    # WiFi 3 (AP3) - AP
    mac.SetType("ns3::ApWifiMac", "Ssid", ns.core.SsidValue(ssid3))
    ap_devices_3 = wifi.Install(phy3, mac, ns.network.NodeContainer(wifi_ap_node_3))


    # --- Mobilidade ---
    mobility = ns.mobility.MobilityHelper()

    spacing = 5.0
    offset_cell = 75.0

    # Cria alocadores de posição separados
    alloc_wifi_1 = create_grid_position_allocator(n_wifi_sta_1_2, spacing, 0.0, 0.0)
    alloc_wifi_2 = create_grid_position_allocator(n_wifi_sta_1_2, spacing, 0.0, offset_cell)
    alloc_wifi_3 = create_grid_position_allocator(n_wifi_sta_3, spacing, offset_cell, 0.0)

    # Instala posições e modelo constante nos STAs
    mobility.SetPositionAllocator(alloc_wifi_1)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel")
    mobility.Install(wifi_sta_nodes_1)

    mobility.SetPositionAllocator(alloc_wifi_2)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel")
    mobility.Install(wifi_sta_nodes_2)

    mobility.SetPositionAllocator(alloc_wifi_3)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel")
    mobility.Install(wifi_sta_nodes_3)

    # Alocador para APs
    ap_alloc = ns.mobility.ListPositionAllocator()

    cols1 = int(math.ceil(math.sqrt(max(1.0, n_wifi_sta_1_2))))
    cols3 = int(math.ceil(math.sqrt(max(1.0, n_wifi_sta_3))))

    # AP1 center
    ap1x = (cols1 * spacing) / 2.0
    ap1y = (cols1 * spacing) / 2.0
    ap_alloc.Add(Vector(ap1x, ap1y, 0.0))

    # AP2 (WiFi2) center - offset in Y
    ap2x = (cols1 * spacing) / 2.0
    ap2y = offset_cell + (cols1 * spacing) / 2.0
    ap_alloc.Add(Vector(ap2x, ap2y, 0.0))
    
    # AP3 (WiFi3) center - offset in X
    ap3x = offset_cell + (cols3 * spacing) / 2.0
    ap3y = (cols3 * spacing) / 2.0
    ap_alloc.Add(Vector(ap3x, ap3y, 0.0))

    mobility.SetPositionAllocator(ap_alloc)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel")

    # APs
    mobility.Install(ns.network.NodeContainer(wifi_ap_node_1))
    mobility.Install(ns.network.NodeContainer(wifi_ap_node_2))
    mobility.Install(ns.network.NodeContainer(wifi_ap_node_3))


    # --- Pilhas, Endereçamento e Roteamento IPv6 ---

    # 1. Roteadores (n0, n1, n2) usam RIPng
    rip_ng = ns.ripng.RipNgHelper()
    list_rh = ns.internet.Ipv6ListRoutingHelper()
    list_rh.Add(rip_ng, 0)

    router_stack = ns.internet.InternetStackHelper()
    router_stack.SetRoutingHelper(list_rh)
    router_stack.Install(p2p_nodes)

    # 2. Nós Finais (STAs) usam Ipv6StaticRouting
    ipv6_static_routing = ns.internet.Ipv6StaticRoutingHelper()
    sta_stack = ns.internet.InternetStackHelper()
    sta_stack.SetRoutingHelper(ipv6_static_routing)

    sta_stack.Install(wifi_sta_nodes_1)
    sta_stack.Install(wifi_sta_nodes_2)
    sta_stack.Install(wifi_sta_nodes_3)

    # Endereçamento IPv6
    address = ns.internet.Ipv6AddressHelper()

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64))  # AP1-AP2
    ap1_ap2_interfaces = address.Assign(ap1_ap2)

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64))  # WiFi1 (AP1)
    wifi_interfaces_1 = address.Assign(sta_devices_1)
    ap_interfaces_1 = address.Assign(ap_devices_1)

    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64))  # WiFi2 (AP2)
    wifi_interfaces_2 = address.Assign(sta_devices_2)
    ap_interfaces_2 = address.Assign(ap_devices_2)

    address.SetBase(Ipv6Address("2001:7::"), Ipv6Prefix(64))  # WiFi3 (AP3)
    wifi_interfaces_3 = address.Assign(sta_devices_3)
    ap_interfaces_3 = address.Assign(ap_devices_3)

    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64))  # AP1-AP3
    ap1_ap3_interfaces = address.Assign(ap1_ap3)

    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64))  # AP2-AP3
    ap2_ap3_interfaces = address.Assign(ap2_ap3)

    # Habilitar Forwarding (Roteamento) nos Roteadores (p2pNodes)
    for i in range(p2p_nodes.GetN()):
        ipv6 = p2p_nodes.Get(i).GetObject(ns.internet.Ipv6.GetTypeId())
        ipv6.SetForwarding(0, True)

    # Rotas estáticas nos STAs (Default Route para o AP)
    
    # Rede 1 (AP1)
    ap1_addr = ap_interfaces_1.GetAddress(0, 1)
    for i in range(wifi_sta_nodes_1.GetN()):
        ipv6 = wifi_sta_nodes_1.Get(i).GetObject(ns.internet.Ipv6.GetTypeId())
        sr = ipv6_static_routing.GetStaticRouting(ipv6)
        # O índice 0 é a interface loopback; a interface WiFi é o índice 1
        if_sta = ipv6.GetInterfaceForDevice(sta_devices_1.Get(i))
        sr.SetDefaultRoute(ap1_addr, if_sta)

    # Rede 2 (AP2)
    ap2_addr = ap_interfaces_2.GetAddress(0, 1)
    for i in range(wifi_sta_nodes_2.GetN()):
        ipv6 = wifi_sta_nodes_2.Get(i).GetObject(ns.internet.Ipv6.GetTypeId())
        sr = ipv6_static_routing.GetStaticRouting(ipv6)
        if_sta = ipv6.GetInterfaceForDevice(sta_devices_2.Get(i))
        sr.SetDefaultRoute(ap2_addr, if_sta)

    # Rede 3 (AP3)
    ap3_addr = ap_interfaces_3.GetAddress(0, 1)
    for i in range(wifi_sta_nodes_3.GetN()):
        ipv6 = wifi_sta_nodes_3.Get(i).GetObject(ns.internet.Ipv6.GetTypeId())
        sr = ipv6_static_routing.GetStaticRouting(ipv6)
        if_sta = ipv6.GetInterfaceForDevice(sta_devices_3.Get(i))
        sr.SetDefaultRoute(ap3_addr, if_sta)
        

    # --- Aplicações de Teste e DDoS ---

    # 1. Configuração do Receptor (Sink) no AP2 (n1) para tráfego normal
    ap2_receptor = wifi_ap_node_2  # AP2 (n1)
    sink_port_normal = 9002
    
    sink_helper_normal = ns.applications.PacketSinkHelper(
        "ns3::UdpSocketFactory",
        ns.network.Inet6SocketAddress(Ipv6Address.GetAny(), sink_port_normal)
    )
    sink_app_normal = sink_helper_normal.Install(ap2_receptor)
    sink_app_normal.Start(ns.core.Seconds(1.5))
    sink_app_normal.Stop(ns.core.Seconds(60.0))

    # 2. Configuração do Emissor (OnOff) para tráfego normal
    ap2_address = ap_interfaces_2.GetAddress(0, 1)  # Endereço do AP2
    
    onoff_normal = ns.applications.OnOffHelper(
        "ns3::UdpSocketFactory",
        ns.network.Address(ns.network.Inet6SocketAddress(ap2_address, sink_port_normal))
    )
    
    onoff_normal.SetAttribute("DataRate", ns.core.StringValue("100kbps"))
    onoff_normal.SetAttribute("PacketSize", ns.core.UintegerValue(1000))
    # OnTime e OffTime configurados para enviar 1 pacote a cada 'interval'
    onoff_normal.SetAttribute("OnTime", ns.core.StringValue("ns3::ConstantRandomVariable[Constant=1]"))
    onoff_normal.SetAttribute("OffTime", ns.core.StringValue("ns3::ConstantRandomVariable[Constant=0]"))

    # 3. Agendamento Sequencial (Nós 61 em diante da Rede 2)
    start_offset = 12.0
    interval = 2.0
    
    # Itera de 61 até o final (n_wifi_sta_1_2)
    for i in range(61, n_wifi_sta_1_2):
        client_app = onoff_normal.Install(wifi_sta_nodes_2.Get(i))
        
        start_time = start_offset + (i - 61) * interval
        client_app.Start(ns.core.Seconds(start_time))
        client_app.Stop(ns.core.Seconds(start_time + 1.0)) # Roda por 1 segundo


    # --- Configuração de Ataque DDoS ---
    
    attacker_nodes = ns.network.NodeContainer()
    for i in range(60): # Nós 0 até 59 da Rede 2
        attacker_nodes.Add(wifi_sta_nodes_2.Get(i))

    victim = wifi_ap_node_2 # AP2
    victim_address = ap_interfaces_2.GetAddress(0, 1)

    attack_port = 9001
    
    # Sink para o tráfego de ataque
    udp_sink_helper_attack = ns.applications.PacketSinkHelper(
        "ns3::UdpSocketFactory",
        ns.network.Inet6SocketAddress(Ipv6Address.GetAny(), attack_port)
    )
    sink_app_attack = udp_sink_helper_attack.Install(victim)
    sink_app_attack.Start(ns.core.Seconds(1.0))
    sink_app_attack.Stop(ns.core.Seconds(60.0))
    
    # Emissores de ataque
    for i in range(attacker_nodes.GetN()):
        onoff_attack = ns.applications.OnOffHelper(
            "ns3::UdpSocketFactory",
            ns.network.Address(ns.network.Inet6SocketAddress(victim_address, attack_port))
        )
        onoff_attack.SetAttribute("DataRate", ns.core.StringValue("5Mbps"))
        onoff_attack.SetAttribute("PacketSize", ns.core.UintegerValue(1024))
        # OnTime/OffTime configurados para enviar constantemente por 15s (OnTime) e parar (OffTime=0)
        onoff_attack.SetAttribute("OnTime", ns.core.StringValue("ns3::ConstantRandomVariable[Constant=15]"))
        onoff_attack.SetAttribute("OffTime", ns.core.StringValue("ns3::ConstantRandomVariable[Constant=0]"))

        # É necessário instalar um OnOffHelper para cada nó, não no container inteiro (como estava no C++)
        attack_app = onoff_attack.Install(attacker_nodes.Get(i))
        attack_app.Start(ns.core.Seconds(15.0))
        attack_app.Stop(ns.core.Seconds(60.0))
        
    ns.core.Simulator.Stop(ns.core.Seconds(60.0))

    if tracing:
        # Trace Pcap para Ponto-a-Ponto
        point_to_point.EnablePcapAll("p2p-traffic-ddos")
        point_to_point.EnablePcap("ddos-p2p-ap2", ap2_ap3.Get(0))

        # Trace Pcap para WiFi
        # Em Python, o DLT_IEEE802_11_RADIO é acessado diretamente
        phy1.SetPcapDataLinkType(WifiPhyHelper.DLT_IEEE802_11_RADIO)
        phy2.SetPcapDataLinkType(WifiPhyHelper.DLT_IEEE802_11_RADIO)
        phy3.SetPcapDataLinkType(WifiPhyHelper.DLT_IEEE802_11_RADIO)

        # Habilita Pcap para os APs
        # ap_devices_X é um NetDeviceContainer de 1 nó
        phy1.EnablePcap("ddos_ap1", ap_devices_1.Get(0))
        phy2.EnablePcap("ddos_ap2", ap_devices_2.Get(0))
        phy3.EnablePcap("ddos_ap3", ap_devices_3.Get(0))

    ns.core.Simulator.Run()
    ns.core.Simulator.Destroy()
    return 0

# Execução do script
if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv))
