// =============================================================================
//  VARREDURA DE BASELINE - DDoS em IEEE 802.15.4 (LR-WPAN) + 6LoWPAN
//  TOPOLOGIA MULTI-PAN  (sem IA)
//
//  Objetivo: achar o "joelho" — a combinacao (nos por PAN x taxa por no) em que
//  a entrega do trafego normal sobe para ~95% SEM ataque. Tres alavancas estao
//  ligadas para tirar o canal do regime de colapso:
//    (1) CSMA-CA mais largo + mais retransmissoes  -> menos colisao
//    (2) ND estatico (STA<->coordenador)           -> zera NS/NA recorrente
//    (3) DAD desligado                              -> sem rajada de ND no boot
//  E os parametros de carga viram linha de comando:
//    --nodesPerPan, --normalRate, --normalPkt, --attack, --staticNd, --tag
//
//  Saidas (nomeadas pela --tag):
//    flowmon_persec_<tag>.csv   (tx/rx por segundo, normal e ataque)
//    ddos-flowmon-<tag>.xml     (agregado: entrega, atraso)
//
//  Alvo: ns-3.40.
// =============================================================================

#include "ns3/flow-monitor-module.h"
#include "ns3/ipv6-flow-classifier.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/neighbor-cache-helper.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DdosSweep");

// ----------------------------------------------------------------------------
//  Estado Global
// ----------------------------------------------------------------------------
static NodeContainer monitoredNodes;
static FlowMonitorHelper flowmonHelper;
static Ptr<FlowMonitor> flowMonitor;
static Ptr<Ipv6FlowClassifier> ipv6Classifier;

static std::ofstream g_flowCsv;
// Mude de g_lastTxB para g_lastTxPkts
static std::map<ns3::FlowId, uint64_t> g_lastTxPkts, g_lastRxPkts;
static std::string g_tag = "run";

void InstallFlowMonitor()
{
    flowMonitor = flowmonHelper.InstallAll();
    ipv6Classifier = DynamicCast<Ipv6FlowClassifier>(flowmonHelper.GetClassifier6());
    if (ipv6Classifier == nullptr) NS_LOG_WARN("Ipv6FlowClassifier indisponivel.");
}

void LogFlowPerSecond()
{
    if (flowMonitor && ipv6Classifier) {
        flowMonitor->CheckForLostPackets();
        auto stats = flowMonitor->GetFlowStats();
        
        // Contadores de PACOTES em vez de BYTES
        double nTxPkts=0, nRxPkts=0, aTxPkts=0, aRxPkts=0; 
        
        for (auto &kv : stats) {
            ns3::FlowId fid = kv.first;
            const auto &fs = kv.second;
            Ipv6FlowClassifier::FiveTuple t = ipv6Classifier->FindFlow(fid);
            
            // Subtrai os pacotes do segundo atual pelos do segundo anterior
            uint64_t dtx = fs.txPackets - g_lastTxPkts[fid];
            uint64_t drx = fs.rxPackets - g_lastRxPkts[fid];
            g_lastTxPkts[fid] = fs.txPackets;
            g_lastRxPkts[fid] = fs.rxPackets;
            
            if (t.destinationPort == 9002) { nTxPkts += dtx; nRxPkts += drx; }
            else if (t.destinationPort == 9001) { aTxPkts += dtx; aRxPkts += drx; }
        }
        double now = Simulator::Now().GetSeconds();
        
        // Grava diretamente os PPS (Pacotes por Segundo) no CSV
        g_flowCsv << now << ","
                  << nTxPkts << "," << nRxPkts << ","
                  << aTxPkts << "," << aRxPkts << "\n";
        g_flowCsv.flush();
    }
    Simulator::Schedule(Seconds(1.0), &LogFlowPerSecond);
}

void SaveFlowMonXml() {
    flowMonitor->CheckForLostPackets();
    flowMonitor->SerializeToXmlFile("ddos-flowmon-attck" + g_tag + ".xml", true, true);
    std::cout << "[INFO] XML gravado: ddos-flowmon-attck" << g_tag << ".xml\n";
}

// =============================================================================
//  MAIN
// =============================================================================
int main(int argc, char* argv[])
{
    LogComponentEnable("DdosSweep", LOG_LEVEL_INFO);

    // ---- Parametros (agora via CLI, para a varredura) ----
    uint32_t nMonitored  = 173;
    uint32_t nodesPerPan = 25;
    std::string normalRate = "200bps";  // taxa por no do trafego normal
    uint32_t normalPkt = 50;            // bytes de payload por pacote normal
    bool attack    = false;             // varredura de baseline: SEM ataque
    bool staticNd  = true;              // (2) ND estatico STA<->coordenador
    bool tracing   = false;             // pcap desligado por padrao (varredura rapida)
    std::string tag = "run";

    CommandLine cmd(__FILE__);
    cmd.AddValue("nodesPerPan", "Nos por PAN/canal 802.15.4", nodesPerPan);
    cmd.AddValue("normalRate",  "Taxa por no do trafego normal (ex: 200bps,100bps,50bps)", normalRate);
    cmd.AddValue("normalPkt",   "Bytes de payload por pacote normal", normalPkt);
    cmd.AddValue("attack",      "Liga o ataque DDoS (varredura de baseline: false)", attack);
    cmd.AddValue("staticNd",    "Pre-instala vizinhos estaticos (zera ND recorrente)", staticNd);
    cmd.AddValue("tracing",     "Habilita pcap", tracing);
    cmd.AddValue("tag",         "Sufixo dos arquivos de saida", tag);
    cmd.Parse(argc, argv);
    g_tag = tag;

    // (3) Desliga DAD: remove a rajada de Neighbor Solicitation no boot.
    Config::SetDefault("ns3::Icmpv6L4Protocol::DAD", BooleanValue(false));

    const uint32_t K = (nMonitored + nodesPerPan - 1) / nodesPerPan;
    NS_LOG_UNCOND("Topologia: " << nMonitored << " nos em " << K << " PANs de ate "
                  << nodesPerPan << " | normalRate=" << normalRate
                  << " | attack=" << attack << " | staticNd=" << staticNd);

    // ---- Nos ----
    monitoredNodes.Create(nMonitored);
    NodeContainer coordinators; coordinators.Create(K);
    NodeContainer serverNode;   serverNode.Create(1);

    // ---- Backbone cabeado (CSMA) ----
    NodeContainer backbone(coordinators, serverNode);
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    NetDeviceContainer csmaDev = csma.Install(backbone);

    // ---- Radios 802.15.4 + 6LoWPAN + mobilidade ----
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    const double spacing = 5.0;
    const double panGap  = 80.0;

    std::vector<NetDeviceContainer> panSix(K);
    std::vector<NetDeviceContainer> panRaw(K);   // dispositivos LrWpan crus (para CSMA tuning + MACs)
    std::vector<uint32_t> panSliceLen(K);
    std::vector<Ptr<NetDevice>> monSix(nMonitored, nullptr);

    for (uint32_t k = 0; k < K; ++k) {
        uint32_t startIdx = k * nodesPerPan;
        uint32_t endIdx   = std::min<uint32_t>(startIdx + nodesPerPan, nMonitored);
        uint32_t sliceLen = endIdx - startIdx;
        panSliceLen[k] = sliceLen;

        NodeContainer panNodes;
        panNodes.Add(coordinators.Get(k));
        for (uint32_t i = startIdx; i < endIdx; ++i) panNodes.Add(monitoredNodes.Get(i));

        LrWpanHelper lrwpan;
        NetDeviceContainer dev = lrwpan.Install(panNodes);
        lrwpan.CreateAssociatedPan(dev, (uint16_t)(k + 1));
        panRaw[k] = dev;

        // ============================================================
        // (1) TUNING CSMA-CA: espalha as transmissoes (menos colisao) e
        //     recupera as que colidem (mais retransmissao).
        // ============================================================
        for (uint32_t di = 0; di < dev.GetN(); ++di) {
            Ptr<LrWpanNetDevice> ld = DynamicCast<LrWpanNetDevice>(dev.Get(di));
            if (!ld) continue;
            ld->GetCsmaCa()->SetMacMinBE(5);            // default 3
            ld->GetCsmaCa()->SetMacMaxBE(8);            // default 5
            ld->GetCsmaCa()->SetMacMaxCSMABackoffs(5);  // <-- CORRIGIDO PARA 5 (Limite do protocolo)
            ld->GetMac()->SetMacMaxFrameRetries(5);     // default 3
        }

        SixLowPanHelper sixlow;
        NetDeviceContainer six = sixlow.Install(dev);
        panSix[k] = six;

        for (uint32_t li = 0; li < sliceLen; ++li)
            monSix[startIdx + li] = six.Get(li + 1);

        uint32_t cols = std::max<uint32_t>(1, (uint32_t)std::ceil(std::sqrt((double)sliceLen)));
        double ox = k * panGap;
        Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
        pos->Add(Vector(ox + (cols * spacing) / 2.0, (cols * spacing) / 2.0, 0.0));
        for (uint32_t li = 0; li < sliceLen; ++li)
            pos->Add(Vector(ox + (li % cols) * spacing, (li / cols) * spacing, 0.0));
        mobility.SetPositionAllocator(pos);
        mobility.Install(panNodes);

        if (tracing)
            lrwpan.EnablePcap("ddos-" + tag + "-pan" + std::to_string(k), dev.Get(0), true);
    }

    Ptr<ListPositionAllocator> spos = CreateObject<ListPositionAllocator>();
    spos->Add(Vector(-30.0, -30.0, 0.0));
    mobility.SetPositionAllocator(spos);
    mobility.Install(serverNode);

    // ---- Pilhas IPv6 ----
    Ipv6StaticRoutingHelper ipv6StaticRouting;
    
    // Remove RIPng to keep the radio silent. 
    InternetStackHelper backboneStack;
    backboneStack.SetRoutingHelper(ipv6StaticRouting);
    backboneStack.Install(backbone);

    InternetStackHelper staStack;
    staStack.SetRoutingHelper(ipv6StaticRouting);
    staStack.Install(monitoredNodes);

    // ---- Enderecamento ----
    Ipv6AddressHelper address;
    address.SetBase(Ipv6Address("2001:100::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer csmaIf = address.Assign(csmaDev);
    Ipv6Address serverAddr = csmaIf.GetAddress(K, 1);

    std::vector<Ipv6Address> panGateway(K);
    std::vector<Ipv6InterfaceContainer> panIfc(K);   // guardamos para o ND estatico
    for (uint32_t k = 0; k < K; ++k) {
        std::ostringstream b; b << "2001:" << std::hex << (k + 1) << "::";
        address.SetBase(Ipv6Address(b.str().c_str()), Ipv6Prefix(64));
        Ipv6InterfaceContainer ifc = address.Assign(panSix[k]);
        panIfc[k] = ifc;
        panGateway[k] = ifc.GetAddress(0, 1);  // coordenador = device 0 da PAN
    }

    for (uint32_t k = 0; k < K; ++k) {
        Ptr<Ipv6> ipv6 = coordinators.Get(k)->GetObject<Ipv6>();
        for (uint32_t ifIndex = 0; ifIndex < ipv6->GetNInterfaces(); ++ifIndex)
            ipv6->SetForwarding(ifIndex, true);
    }

    for (uint32_t i = 0; i < nMonitored; ++i) {
        uint32_t k = i / nodesPerPan;
        Ptr<Ipv6> ipv6 = monitoredNodes.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(monSix[i]);
        sr->SetDefaultRoute(panGateway[k], ifSta);
    }

    // ---- Global Static Routing ----
    Ptr<Ipv6StaticRouting> serverRoute = ipv6StaticRouting.GetStaticRouting(serverNode.Get(0)->GetObject<Ipv6>());
    for (uint32_t k = 0; k < K; ++k) {
        std::ostringstream b; b << "2001:" << std::hex << (k + 1) << "::";
        // Tell the server to reach PAN 'k' via the specific coordinator on the CSMA backbone
        serverRoute->AddNetworkRouteTo(Ipv6Address(b.str().c_str()), Ipv6Prefix(64), csmaIf.GetAddress(k, 1), 1); 
    }

    // ============================================================
    // (2) ND ESTATICO: Usa o Helper oficial do ns-3 para preencher
    //     todas as tabelas de vizinhos IPv6 da simulação instantaneamente.
    //     Zera as falhas de Neighbor Solicitation/Advertisement no boot.
    // ============================================================
    if (staticNd) {
        NeighborCacheHelper neighborCache;
        neighborCache.PopulateNeighborCache();
        NS_LOG_UNCOND("[INFO] ND estatico instalado globalmente via NeighborCacheHelper.");
    }

    // ================================================================
    //  Atacantes (so usados se --attack=true)
    // ================================================================
    const uint32_t nAtt = 40;
    std::vector<uint32_t> attackerIdx;
    std::set<uint32_t> attackerSet;
    for (uint32_t j = 0; j < nAtt; ++j) {
        uint32_t idx = (uint32_t)std::llround((double)j * nMonitored / nAtt);
        if (idx >= nMonitored) idx = nMonitored - 1;
        if (attackerSet.insert(idx).second) attackerIdx.push_back(idx);
    }

    // ---- Sinks na vitima ----
    uint16_t normalPort = 9002, attackPort = 9001;
    PacketSinkHelper sinkNormal("ns3::UdpSocketFactory", Inet6SocketAddress(Ipv6Address::GetAny(), normalPort));
    PacketSinkHelper sinkAttack("ns3::UdpSocketFactory", Inet6SocketAddress(Ipv6Address::GetAny(), attackPort));
    ApplicationContainer s1 = sinkNormal.Install(serverNode.Get(0));
    ApplicationContainer s2 = sinkAttack.Install(serverNode.Get(0));
    s1.Start(Seconds(1.0)); s1.Stop(Seconds(900.0));
    s2.Start(Seconds(1.0)); s2.Stop(Seconds(900.0));

    // ================================================================
    //  Trafego NORMAL: OnOff com Tempo Exponencial (Fim do Sincronismo)
    // ================================================================
    // Para evitar o efeito "Thundering Herd" (todos os nós a tentarem 
    // enviar pacotes ao mesmo tempo devido ao relógio fixo), usamos o
    // OnOffHelper com janelas Exponenciais. A carga torna-se suave, 
    // constante e perfeitamente absorvível pelo rádio de 250 kbps.

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(Inet6SocketAddress(serverAddr, normalPort)));
    
    // O rádio envia à velocidade real da antena. O que dita o volume de tráfego 
    // é a percentagem de tempo que a aplicação passa no estado "On".
    onoff.SetAttribute("DataRate", StringValue("200bps")); 
    onoff.SetAttribute("PacketSize", UintegerValue(20));

    // Exponencial: A média do tempo On é muito curta (0.1s), tempo Off é longo (4.0s).
    // O rádio só está a injetar dados 2% do tempo total.
    onoff.SetAttribute("OnTime",  StringValue("ns3::ExponentialRandomVariable[Mean=0.1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=4.0]"));

    // O arranque continua aleatório
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    uv->SetAttribute("Min", DoubleValue(0.0));
    uv->SetAttribute("Max", DoubleValue(4.0)); 

    for (uint32_t i = 0; i < nMonitored; ++i) {
        ApplicationContainer app = onoff.Install(monitoredNodes.Get(i));
        app.Start(Seconds(1.0 + uv->GetValue()));
        app.Stop(Seconds(900.0));
    }
    // ---- ATAQUE (opcional) ----
    if (attack) {
        OnOffHelper onoffAtk("ns3::UdpSocketFactory", Address(Inet6SocketAddress(serverAddr, attackPort)));
        onoffAtk.SetAttribute("DataRate",   StringValue("128kbps"));
        onoffAtk.SetAttribute("PacketSize", UintegerValue(1000));
        onoffAtk.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=15]"));
        onoffAtk.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        for (uint32_t j = 0; j < attackerIdx.size(); ++j) {
            ApplicationContainer app = onoffAtk.Install(monitoredNodes.Get(attackerIdx[j]));
            if (j % 2 == 0) { app.Start(Seconds(170.0 + (j/2)*0.05)); app.Stop(Seconds(220.0)); }
            else            { app.Start(Seconds(250.0 + (j/2)*0.05)); app.Stop(Seconds(300.0)); }
        }
    }

    // ---- FlowMonitor + logging ----
    InstallFlowMonitor();
    Simulator::Schedule(Seconds(899.9), &SaveFlowMonXml);
    g_flowCsv.open("flowmon_persec_attack" + tag + ".csv");
    // Novo cabeçalho:
    g_flowCsv << "tempo,normal_tx_pps,normal_rx_pps,ataque_tx_pps,ataque_rx_pps\n";
    Simulator::Schedule(Seconds(1.0), &LogFlowPerSecond);

    if (tracing)
        csma.EnablePcap("ddos-" + tag + "-server", csmaDev.Get(K), true);

    Simulator::Stop(Seconds(901.0));
    Simulator::Run();
    g_flowCsv.close();
    Simulator::Destroy();
    return 0;
}
