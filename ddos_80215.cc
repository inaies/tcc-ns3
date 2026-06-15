// =============================================================================
//  DDoS detection em ns-3 + ns3-gym, IEEE 802.15.4 (LR-WPAN) + 6LoWPAN
//  TOPOLOGIA MULTI-PAN
//
//  Os 173 nos OBSERVADOS sao divididos em K PANs pequenas (~25 nos cada),
//  cada uma em seu proprio canal/coordenador. Os coordenadores e o servidor
//  (vitima) ficam num BACKBONE CABEADO (CSMA), de alta capacidade.
//
//  Por que isso evita o colapso visto antes:
//   - Cada canal 802.15.4 (250 kbps) carrega so ~25 nos -> ND estabiliza e
//     sobra banda para o trafego de aplicacao.
//   - O trafego de todas as PANs em direcao a vitima e agregado no backbone
//     CSMA (100 Mbps), nao no radio -> nenhum canal de radio vira gargalo.
//
//  A dimensao do espaco de observacao/acao do agente continua 173: o vetor e
//  indexado pelo no monitorado (monitoredNodes.Get(i)), independente da PAN.
//
//  Versao alvo: ns-3.40 (classes LrWpan* no namespace ns3; associacao via
//  atribuicao direta de PanId/short address, robusta para qualquer densidade).
// =============================================================================

#include "ns3/opengym-module.h"
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

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>
#include <fstream>   // se ainda nao tiver


NS_LOG_COMPONENT_DEFINE("DdosOpengym");

using namespace ns3;
// using namespace ns3::lrwpan;   // <- descomente em ns-3 >= 3.43

// ----------------------------------------------------------------------------
//  Estado global usado pelos callbacks do Gym
// ----------------------------------------------------------------------------
static NodeContainer monitoredNodes;       // os 173 nos observados (flat, 0..172)
static uint32_t g_nNodes = 173;            // dimensao da observacao/acao

static FlowMonitorHelper flowmonHelper;
static Ptr<FlowMonitor> flowMonitor;
static Ptr<Ipv6FlowClassifier> ipv6Classifier;
static std::map<uint32_t, uint64_t> lastRxBytesPerFlow;

static std::ofstream g_flowCsv;
static std::map<ns3::FlowId, uint64_t> g_lastTxB, g_lastRxB;
static std::string g_tag = "run";
// ----------------------------------------------------------------------------
//  FlowMonitor
// ----------------------------------------------------------------------------
void InstallFlowMonitor()
{
    flowMonitor = flowmonHelper.InstallAll();
    ipv6Classifier = DynamicCast<Ipv6FlowClassifier>(flowmonHelper.GetClassifier6());
    if (ipv6Classifier == nullptr) {
        NS_LOG_WARN("Ipv6FlowClassifier indisponivel; mapeamento fluxo->endereco ausente.");
    }
    lastRxBytesPerFlow.clear();
}

std::map<std::string, double> CollectNodeThroughputs(double intervalSeconds)
{
    std::map<std::string, double> throughputBySrc;
    if (!flowMonitor) return throughputBySrc;

    flowMonitor->CheckForLostPackets();
    auto stats = flowMonitor->GetFlowStats();
    if (ipv6Classifier == nullptr) return throughputBySrc;

    for (auto &kv : stats) {
        FlowId fid = kv.first;
        Ipv6FlowClassifier::FiveTuple t = ipv6Classifier->FindFlow(fid);
        std::ostringstream oss; oss << t.sourceAddress;
        std::string src = oss.str();

    uint64_t txBytes = kv.second.txBytes; 
        uint64_t prev = lastRxBytesPerFlow.count(fid) ? lastRxBytesPerFlow[fid] : 0;
        uint64_t delta = (txBytes >= prev) ? (txBytes - prev) : 0;
        lastRxBytesPerFlow[fid] = txBytes;
        throughputBySrc[src] += (double)delta / intervalSeconds;
    }
    return throughputBySrc;
}

// ----------------------------------------------------------------------------
//  Callbacks do OpenGym (indexados pelo no monitorado, 0..g_nNodes-1)
// ----------------------------------------------------------------------------
Ptr<OpenGymSpace> MyGetObservationSpace(void)
{
    std::vector<uint32_t> shape = {g_nNodes};
    return CreateObject<OpenGymBoxSpace>(0.0, 1e9, shape, TypeNameGet<float>());
}

Ptr<OpenGymSpace> MyGetActionSpace(void)
{
    std::vector<uint32_t> shape = {g_nNodes};
    std::vector<float> low(g_nNodes, 0.0f), high(g_nNodes, 1.0f);
    return CreateObject<OpenGymBoxSpace>(low, high, shape, "float32");
}

Ptr<OpenGymDataContainer> MyGetObservation(void)
{
    std::vector<uint32_t> shape = {g_nNodes};
    Ptr<OpenGymBoxContainer<float>> box = CreateObject<OpenGymBoxContainer<float>>(shape);
    std::map<std::string, double> tpMap = CollectNodeThroughputs(1.0);

    for (uint32_t i = 0; i < g_nNodes && i < monitoredNodes.GetN(); i++) {
        float val = 0.0f;
        Ptr<Ipv6> ipv6 = monitoredNodes.Get(i)->GetObject<Ipv6>();
        if (ipv6) {
            for (uint32_t ifIdx = 0; ifIdx < ipv6->GetNInterfaces(); ++ifIdx) {
                if (ipv6->GetNAddresses(ifIdx) < 2) continue;
                std::ostringstream oss; oss << ipv6->GetAddress(ifIdx, 1).GetAddress();
                if (tpMap.count(oss.str())) val = (float)tpMap[oss.str()];
            }
        }
        box->AddValue(val);
    }

    std::vector<float> data = box->GetData();
    std::stringstream ss; ss << "[";
    for (size_t i = 0; i < data.size(); ++i) { ss << data[i]; if (i+1<data.size()) ss << ", "; }
    ss << "]";
    NS_LOG_UNCOND("MyGetObservation [t=" << Simulator::Now().GetSeconds() << "s]: " << ss.str());
    return box;
}

float MyGetReward(void) { return 1.0; }
bool  MyGetGameOver(void) { return Now().GetSeconds() >= 900.0; }

std::string MyGetExtraInfo(void)
{
    std::stringstream ss;
    for (uint32_t i = 0; i < monitoredNodes.GetN(); ++i) {
        Ptr<Ipv6> ipv6 = monitoredNodes.Get(i)->GetObject<Ipv6>();
        if (!ipv6) continue;
        for (uint32_t ifIdx = 0; ifIdx < ipv6->GetNInterfaces(); ++ifIdx) {
            if (ipv6->GetNAddresses(ifIdx) < 2) continue;
            ss << i << "=" << ipv6->GetAddress(ifIdx, 1).GetAddress() << "|";
            break;
        }
    }
    return ss.str();
}

bool MyExecuteActions(Ptr<OpenGymDataContainer> action)
{
    if (!action) return false;
    Ptr<OpenGymBoxContainer<float>> box = DynamicCast<OpenGymBoxContainer<float>>(action);
    if (!box) return false;
    std::vector<float> actions = box->GetData();

    for (uint32_t i = 0; i < actions.size() && i < monitoredNodes.GetN(); ++i) {
        Ptr<Ipv6> ipv6 = monitoredNodes.Get(i)->GetObject<Ipv6>();
        if (!ipv6) continue;
        bool isolate = actions[i] > 0.5f;
        for (uint32_t ifIndex = 1; ifIndex < ipv6->GetNInterfaces(); ++ifIndex) {
            if (isolate && ipv6->IsUp(ifIndex))  ipv6->SetDown(ifIndex);
            if (!isolate && !ipv6->IsUp(ifIndex)) ipv6->SetUp(ifIndex);
        }
    }
    return true;
}

void ScheduleNextStateRead(double envStepTime, Ptr<OpenGymInterface> openGym)
{
    openGym->NotifyCurrentState();
    Simulator::Schedule(Seconds(envStepTime), &ScheduleNextStateRead, envStepTime, openGym);
}

void LogFlowPerSecond()
{
    if (flowMonitor && ipv6Classifier) {
        flowMonitor->CheckForLostPackets();
        auto stats = flowMonitor->GetFlowStats();
        double nTx=0, nRx=0, aTx=0, aRx=0;
        for (auto &kv : stats) {
            ns3::FlowId fid = kv.first;
            const auto &fs = kv.second;
            Ipv6FlowClassifier::FiveTuple t = ipv6Classifier->FindFlow(fid);
            uint64_t dtx = fs.txBytes - g_lastTxB[fid];
            uint64_t drx = fs.rxBytes - g_lastRxB[fid];
            g_lastTxB[fid] = fs.txBytes;
            g_lastRxB[fid] = fs.rxBytes;
            if (t.destinationPort == 9002) { nTx += dtx; nRx += drx; }
            else if (t.destinationPort == 9001) { aTx += dtx; aRx += drx; }
        }
        double now = Simulator::Now().GetSeconds();
        g_flowCsv << now << ","
                  << nTx*8.0/1000.0 << "," << nRx*8.0/1000.0 << ","
                  << aTx*8.0/1000.0 << "," << aRx*8.0/1000.0 << "\n";
        g_flowCsv.flush();
    }
    Simulator::Schedule(Seconds(1.0), &LogFlowPerSecond);
}

void SaveFlowMonXml() {
    flowMonitor->CheckForLostPackets();
    flowMonitor->SerializeToXmlFile("ddos-flowmon-system-" + g_tag + ".xml", true, true);
    std::cout << "[INFO] XML gravado: ddos-flowmon-system-" << g_tag << ".xml\n";
}

// =============================================================================
//  MAIN
// =============================================================================
int main(int argc, char* argv[])
{
    LogComponentEnable("DdosOpengym", LOG_LEVEL_INFO);

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
    RipNgHelper ripNg;
    Ipv6ListRoutingHelper listRh;
    listRh.Add(ipv6StaticRouting, 10);
    listRh.Add(ripNg, 0);

    InternetStackHelper backboneStack;
    backboneStack.SetRoutingHelper(listRh);
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

    // ---- Trafego NORMAL (taxa parametrizada; on continuo + start aleatorio) ----
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(Inet6SocketAddress(serverAddr, normalPort)));
    
    // Voltamos para 500 bps (o que gera 12.5 kbps por PAN, garantindo canal livre)
    onoff.SetAttribute("DataRate", StringValue("500bps")); 
    onoff.SetAttribute("PacketSize", UintegerValue(50));
    
    // Como 50 bytes (400 bits) a 500bps demoram 0.8 segundos a enviar:
    onoff.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=0.8]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=1.5|Max=2.34]"));

    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    uv->SetAttribute("Min", DoubleValue(0.0));
    uv->SetAttribute("Max", DoubleValue(2.0));
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
    g_flowCsv.open("flowmon_persec_system.csv");
    g_flowCsv << "tempo,normal_tx_kbps,normal_rx_kbps,ataque_tx_kbps,ataque_rx_kbps\n";
    Simulator::Schedule(Seconds(1.0), &LogFlowPerSecond);

    uint32_t openGymPort = 5555;
    double envStepTime = 1.0;
    Ptr<OpenGymInterface> openGym = CreateObject<OpenGymInterface>(openGymPort);
    openGym->SetGetObservationSpaceCb(MakeCallback(&MyGetObservationSpace));
    openGym->SetGetActionSpaceCb(MakeCallback(&MyGetActionSpace));
    openGym->SetGetObservationCb(MakeCallback(&MyGetObservation));
    openGym->SetGetRewardCb(MakeCallback(&MyGetReward));
    openGym->SetGetGameOverCb(MakeCallback(&MyGetGameOver));
    openGym->SetGetExtraInfoCb(MakeCallback(&MyGetExtraInfo));
    openGym->SetExecuteActionsCb(MakeCallback(&MyExecuteActions));
    Simulator::Schedule(Seconds(0.0), &ScheduleNextStateRead, envStepTime, openGym);

    if (tracing)
        csma.EnablePcap("ddos-server", csmaDev.Get(K), true); // visao agregada na vitima

    Simulator::Stop(Seconds(901.0));
    Simulator::Run();

    g_flowCsv.close();

    Simulator::Destroy();
    return 0;
}