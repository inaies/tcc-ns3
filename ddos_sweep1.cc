// =============================================================================
//  DDoS detection em ns-3 + ns3-gym, IEEE 802.15.4 (LR-WPAN) + 6LoWPAN
//  TOPOLOGIA: UM AP CENTRAL (gateway + sistema de deteccao)
//
//  Um unico no "AP" possui K radios 802.15.4, um por canal. Cada canal atende
//  ~25 dispositivos IIoT. Todos os dispositivos enviam direto para o AP (que e
//  o sink). NAO ha coordenadores intermediarios nem backbone cabeado.
//
//  Por que K radios em vez de 1: um unico radio/canal de 250 kbps NAO comporta
//  173 nos (colapso de canal ja medido). Separar por canal (frequencia) e a
//  unica forma de manter os 173 dispositivos sob um AP fisicamente viavel.
//  Os radios ficam todos no MESMO no (o AP), entao continua sendo 1 AP central.
//
//  Deteccao: o agente (OpenGym) observa a carga OFERTADA por dispositivo
//  (txBytes) -- e nao o que chega -- porque o flood de saturacao de canal morre
//  no ar antes de chegar ao AP. A acao de isolamento desliga a interface do no.
//
//  Alvo: ns-3.40.
// =============================================================================

#include "ns3/opengym-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv6-flow-classifier.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/neighbor-cache-helper.h"
#include "ns3/traffic-control-module.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DdosApCentral");

// ----------------------------------------------------------------------------
//  Estado global
// ----------------------------------------------------------------------------
static NodeContainer monitoredNodes;        // os 173 dispositivos IIoT (flat)
static uint32_t g_nNodes = 173;             // dimensao observacao/acao
static Ptr<Node> g_ap;                      // o AP central

static FlowMonitorHelper flowmonHelper;
static Ptr<FlowMonitor> flowMonitor;
static Ptr<Ipv6FlowClassifier> ipv6Classifier;
static std::map<uint32_t, uint64_t> lastTxBytesPerFlow;

static std::ofstream g_flowCsv;
static std::map<ns3::FlowId, uint64_t> g_lastTxB, g_lastRxB;

// Contadores de descarte (diagnostico: onde o pacote morre)
static uint64_t g_macTxDrop=0, g_macRxDrop=0, g_phyRxDrop=0, g_sixDrop=0;
static void MacTxDropCb(Ptr<const Packet>) { g_macTxDrop++; }
static void MacRxDropCb(Ptr<const Packet>) { g_macRxDrop++; }
static void PhyRxDropCb(Ptr<const Packet>) { g_phyRxDrop++; }
static void SixDropCb(SixLowPanNetDevice::DropReason, Ptr<const Packet>, Ptr<SixLowPanNetDevice>, uint32_t) { g_sixDrop++; }
static uint64_t g_queueDrop=0;
static void QueueDropCb(Ptr<const QueueDiscItem> item) { g_queueDrop++; }

void ImprimirDescartes() {
    flowMonitor->CheckForLostPackets();
    auto stats = flowMonitor->GetFlowStats();
    uint64_t tx=0, rx=0;
    for (auto &kv : stats) {
        Ipv6FlowClassifier::FiveTuple t = ipv6Classifier->FindFlow(kv.first);
        if (t.destinationPort == 9002) { tx += kv.second.txPackets; rx += kv.second.rxPackets; }
    }
    std::cout << "\n=== PERDA DO TRAFEGO NORMAL (porta 9002) ===\n";
    if (tx) std::cout << "TX=" << tx << "  RX=" << rx
                      << "  PERDA=" << 100.0*(tx-rx)/(double)tx << "%\n";
    std::cout << "=== ONDE OS PACOTES FORAM DESCARTADOS? ===\n"
              << "Fila RAM (Overflow do Traffic Control) : " << g_queueDrop << "\n" // <-- Adicionado!
              << "MacTxDrop (colisao / sem ACK)          : " << g_macTxDrop << "\n"
              << "MacRxDrop (fila / malformado)          : " << g_macRxDrop << "\n"
              << "PhyRxDrop (interferencia)              : " << g_phyRxDrop << "\n"
              << "SixLowPan Drop (fragmentacao)          : " << g_sixDrop  << "\n";
}

void InstallFlowMonitor() {
    flowMonitor = flowmonHelper.InstallAll();
    ipv6Classifier = DynamicCast<Ipv6FlowClassifier>(flowmonHelper.GetClassifier6());
    if (!ipv6Classifier) NS_LOG_WARN("Ipv6FlowClassifier indisponivel.");
    lastTxBytesPerFlow.clear();
}

// Observacao = carga OFERTADA por origem (txBytes), nao a entregue.
std::map<std::string, double> CollectNodeThroughputs(double intervalSeconds) {
    std::map<std::string, double> tpBySrc;
    if (!flowMonitor || !ipv6Classifier) return tpBySrc;
    flowMonitor->CheckForLostPackets();
    auto stats = flowMonitor->GetFlowStats();
    for (auto &kv : stats) {
        ns3::FlowId fid = kv.first;
        Ipv6FlowClassifier::FiveTuple t = ipv6Classifier->FindFlow(fid);
        std::ostringstream oss; oss << t.sourceAddress;
        uint64_t txBytes = kv.second.txBytes;
        uint64_t prev = lastTxBytesPerFlow.count(fid) ? lastTxBytesPerFlow[fid] : 0;
        uint64_t delta = (txBytes >= prev) ? (txBytes - prev) : 0;
        lastTxBytesPerFlow[fid] = txBytes;
        tpBySrc[oss.str()] += (double)delta / intervalSeconds;
    }
    return tpBySrc;
}

// ---- OpenGym (indexado pelo dispositivo, 0..g_nNodes-1) ----
Ptr<OpenGymSpace> MyGetObservationSpace() {
    std::vector<uint32_t> shape = {g_nNodes};
    return CreateObject<OpenGymBoxSpace>(0.0, 1e9, shape, TypeNameGet<float>());
}
Ptr<OpenGymSpace> MyGetActionSpace() {
    std::vector<uint32_t> shape = {g_nNodes};
    std::vector<float> low(g_nNodes, 0.0f), high(g_nNodes, 1.0f);
    return CreateObject<OpenGymBoxSpace>(low, high, shape, "float32");
}
Ptr<OpenGymDataContainer> MyGetObservation() {
    std::vector<uint32_t> shape = {g_nNodes};
    Ptr<OpenGymBoxContainer<float>> box = CreateObject<OpenGymBoxContainer<float>>(shape);
    auto tpMap = CollectNodeThroughputs(1.0);
    for (uint32_t i = 0; i < g_nNodes && i < monitoredNodes.GetN(); i++) {
        float val = 0.0f;
        Ptr<Ipv6> ipv6 = monitoredNodes.Get(i)->GetObject<Ipv6>();
        if (ipv6) for (uint32_t ifIdx = 0; ifIdx < ipv6->GetNInterfaces(); ++ifIdx) {
            if (ipv6->GetNAddresses(ifIdx) < 2) continue;
            std::ostringstream oss; oss << ipv6->GetAddress(ifIdx, 1).GetAddress();
            if (tpMap.count(oss.str())) val = (float)tpMap[oss.str()];
        }
        box->AddValue(val);
    }
    return box;
}
float MyGetReward() { return 1.0; }
bool  MyGetGameOver() { return Now().GetSeconds() >= 900.0; }
std::string MyGetExtraInfo() {
    std::stringstream ss;
    for (uint32_t i = 0; i < monitoredNodes.GetN(); ++i) {
        Ptr<Ipv6> ipv6 = monitoredNodes.Get(i)->GetObject<Ipv6>();
        if (!ipv6) continue;
        for (uint32_t ifIdx = 0; ifIdx < ipv6->GetNInterfaces(); ++ifIdx) {
            if (ipv6->GetNAddresses(ifIdx) < 2) continue;
            ss << i << "=" << ipv6->GetAddress(ifIdx, 1).GetAddress() << "|"; break;
        }
    }
    return ss.str();
}
bool MyExecuteActions(Ptr<OpenGymDataContainer> action) {
    if (!action) return false;
    Ptr<OpenGymBoxContainer<float>> box = DynamicCast<OpenGymBoxContainer<float>>(action);
    if (!box) return false;
    std::vector<float> actions = box->GetData();
    
    for (uint32_t i = 0; i < actions.size() && i < monitoredNodes.GetN(); ++i) {
        bool isolate = actions[i] > 0.5f;
        Ptr<Node> node = monitoredNodes.Get(i);

        // =================================================================
        // ESTRATÉGIA DE ISOLAMENTO: Host IPS / Throttling
        // Mantém a estabilidade absoluta do simulador ns-3. Em vez de 
        // desligar a antena (SetDown), estrangulamos a aplicação para 1bps.
        // =================================================================
        for (uint32_t a = 0; a < node->GetNApplications(); ++a) {
            Ptr<OnOffApplication> onoff = DynamicCast<OnOffApplication>(node->GetApplication(a));
            if (onoff) {
                if (isolate) {
                    // IA Isolou o nó: Bloqueio virtual (1bps evita erro matemático de divisão por zero)
                    onoff->SetAttribute("DataRate", StringValue("1bps")); 
                } else {
                    // IA Libertou o nó: Restaura a banda
                    if (a == 0) {
                        onoff->SetAttribute("DataRate", StringValue("50kbps")); // Tráfego Paz
                    } else if (a == 1) {
                        onoff->SetAttribute("DataRate", StringValue("128kbps")); // Tráfego Ataque
                    }
                }
            }
        }
    }
    return true;
}

void LogFlowPerSecond() {
    if (flowMonitor && ipv6Classifier) {
        flowMonitor->CheckForLostPackets();
        auto stats = flowMonitor->GetFlowStats();
        double nTx=0,nRx=0,aTx=0,aRx=0;
        for (auto &kv : stats) {
            ns3::FlowId fid = kv.first;
            const auto &fs = kv.second;
            Ipv6FlowClassifier::FiveTuple t = ipv6Classifier->FindFlow(fid);
            
            uint64_t dtx = fs.txPackets - g_lastTxB[fid];
            uint64_t drx = fs.rxPackets - g_lastRxB[fid];
            g_lastTxB[fid] = fs.txPackets; 
            g_lastRxB[fid] = fs.rxPackets;
            
            if (t.destinationPort == 9002) { nTx += dtx; nRx += drx; }
            else if (t.destinationPort == 9001) { aTx += dtx; aRx += drx; }
        }
        double now = Simulator::Now().GetSeconds();
        
        // GRAVAÇÃO CORRIGIDA: Exporta Pacotes por Segundo (PPS) puros!
        g_flowCsv << now << "," << nTx << "," << nRx << "," << aTx << "," << aRx << "\n";
        g_flowCsv.flush();
    }
    Simulator::Schedule(Seconds(1.0), &LogFlowPerSecond);
}
void ScheduleNextStateRead(double envStepTime, Ptr<OpenGymInterface> openGym) {
    openGym->NotifyCurrentState();
    Simulator::Schedule(Seconds(envStepTime), &ScheduleNextStateRead, envStepTime, openGym);
}
void SaveFlowMonXml(std::string tag) {
    flowMonitor->CheckForLostPackets();
    flowMonitor->SerializeToXmlFile("ddos-flowmon-sweep1" + tag + ".xml", true, true);
    std::cout << "[INFO] XML gravado: ddos-flowmon-sweep1" << tag << ".xml\n";
}

// =============================================================================
//  MAIN
// =============================================================================
int main(int argc, char* argv[]) {
    LogComponentEnable("DdosApCentral", LOG_LEVEL_INFO);

    uint32_t nMonitored = 173;
    uint32_t nodesPerPan = 25;       // dispositivos por canal (~25)
    std::string normalRate = "200bps";
    uint32_t normalPkt = 50;
    bool attack    = true;
    bool staticNd  = true;
    uint32_t radioQueue = 8;
    bool tracing   = false;
    std::string tag = "apcentral";

    Config::SetDefault("ns3::Icmpv6L4Protocol::DAD", BooleanValue(false));

    CommandLine cmd(__FILE__);
    cmd.AddValue("nodesPerPan", "Dispositivos por canal 802.15.4", nodesPerPan);
    cmd.AddValue("normalRate",  "Taxa por dispositivo (ex: 200bps,100bps,67bps)", normalRate);
    cmd.AddValue("normalPkt",   "Bytes de payload por pacote normal", normalPkt);
    cmd.AddValue("attack",      "Liga o ataque DDoS", attack);
    cmd.AddValue("staticNd",    "Popula neighbor cache (ND estatico)", staticNd);
    cmd.AddValue("radioQueue",  "Fila do radio em pacotes (0 = default)", radioQueue);
    cmd.AddValue("tracing",     "Habilita pcap", tracing);
    cmd.AddValue("tag",         "Sufixo dos arquivos de saida", tag);
    cmd.Parse(argc, argv);

    g_nNodes = nMonitored;
    const uint32_t K = (nMonitored + nodesPerPan - 1) / nodesPerPan;
    NS_LOG_UNCOND("AP central com " << K << " radios (canais), "
                  << nMonitored << " dispositivos | normalRate=" << normalRate
                  << " | attack=" << attack);

    // ---- Nos: 173 dispositivos + 1 AP central ----
    monitoredNodes.Create(nMonitored);
    NodeContainer apNode; apNode.Create(1);
    g_ap = apNode.Get(0);

    // AP fixo no centro; todos os dispositivos ficam ao redor, no alcance.
    MobilityHelper apMob;
    apMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
    apPos->Add(Vector(0.0, 0.0, 0.0));
    apMob.SetPositionAllocator(apPos);
    apMob.Install(apNode);

    const double spacing = 1.0; // Era 5.0. Agora a distância máxima entre sensores é 5.6 metros!
    const uint32_t cols = 5;

    std::vector<NetDeviceContainer> panSix(K);
    std::vector<uint32_t> panSliceLen(K);
    std::vector<Ptr<NetDevice>> monSix(nMonitored, nullptr);

    // ---- Um radio do AP por canal; cada canal = uma PAN de ~25 dispositivos ----
    for (uint32_t k = 0; k < K; ++k) {
        uint32_t startIdx = k * nodesPerPan;
        uint32_t endIdx   = std::min<uint32_t>(startIdx + nodesPerPan, nMonitored);
        uint32_t sliceLen = endIdx - startIdx;
        panSliceLen[k] = sliceLen;

        NodeContainer panNodes;
        panNodes.Add(apNode.Get(0));   // AP = device 0 do canal k (coordenador)
        NodeContainer devSlice;
        for (uint32_t i = startIdx; i < endIdx; ++i) { 
            panNodes.Add(monitoredNodes.Get(i)); 
            devSlice.Add(monitoredNodes.Get(i)); 
        }

        // ============================================================
        // PASSO 1: MOBILIDADE (Tem de ser ANTES do rádio!)
        // ============================================================
        Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
        for (uint32_t li = 0; li < sliceLen; ++li) {
            double x = ((double)(li % cols) - (cols-1)/2.0) * spacing + k*0.6;
            double y = ((double)(li / cols) - (cols-1)/2.0) * spacing + k*0.6;
            pos->Add(Vector(x, y, 0.0));
        }
        MobilityHelper devMob;
        devMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        devMob.SetPositionAllocator(pos);
        devMob.Install(devSlice);

        // ============================================================
        // PASSO 2: RÁDIO E CANAL FÍSICO ISOLADO (FÍSICA CORRIGIDA)
        // ============================================================
        LrWpanHelper lrwpan;
        
        // AO NÃO CHAMAR "SetChannel", O NS-3 CRIA AUTOMATICAMENTE UM CANAL 
        // ISOLADO PARA CADA PAN, INCLUINDO AS LEIS DA FÍSICA (DELAY E LOSS)!
        
        NetDeviceContainer dev = lrwpan.Install(panNodes);
        lrwpan.CreateAssociatedPan(dev, (uint16_t)(k + 1));

        // CSMA-CA tuning 
        for (uint32_t di = 0; di < dev.GetN(); ++di) {
            Ptr<LrWpanNetDevice> ld = DynamicCast<LrWpanNetDevice>(dev.Get(di));
            if (!ld) continue;
            ld->GetCsmaCa()->SetMacMinBE(5);
            ld->GetCsmaCa()->SetMacMaxBE(8);
            ld->GetCsmaCa()->SetMacMaxCSMABackoffs(5);  // max permitido = 5
            ld->GetMac()->SetMacMaxFrameRetries(5);
        }

        SixLowPanHelper sixlow;
        NetDeviceContainer six = sixlow.Install(dev);
        panSix[k] = six;
        for (uint32_t li = 0; li < sliceLen; ++li)
            monSix[startIdx + li] = six.Get(li + 1);

        if (tracing)
            lrwpan.EnablePcap("ddos-" + tag + "-ch" + std::to_string(k) + "-ap", dev.Get(0), true);
    }

    // ---- Pilha IPv6 (so roteamento estatico; sem backbone, sem RIPng) ----
    InternetStackHelper stack;          // AP e dispositivos
    stack.Install(apNode);
    stack.Install(monitoredNodes);

    // ---- Fila pequena no radio (antes de enderecar) ----
    if (radioQueue > 0) {
        TrafficControlHelper tch;
        tch.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize",
                             StringValue(std::to_string(radioQueue) + "p"));
        for (uint32_t k = 0; k < K; ++k) tch.Install(panSix[k]);
    }

    // ---- Enderecamento: canal k em 2001:(k+1)::/64 ----
    //  AP = device 0 de cada canal. Dispositivos ficam no MESMO /64 do AP
    //  (on-link) -> falam direto com o AP, sem rotas nem forwarding.
    Ipv6AddressHelper address;
    std::vector<Ipv6Address> apAddr(K);   // endereco do AP em cada canal
    for (uint32_t k = 0; k < K; ++k) {
        std::ostringstream b; b << "2001:" << std::hex << (k + 1) << "::";
        address.SetBase(Ipv6Address(b.str().c_str()), Ipv6Prefix(64));
        Ipv6InterfaceContainer ifc = address.Assign(panSix[k]);
        apAddr[k] = ifc.GetAddress(0, 1);   // AP
    }

    if (staticNd) {
        NeighborCacheHelper neighborCache;
        neighborCache.PopulateNeighborCache();
        NS_LOG_UNCOND("[INFO] Neighbor cache populada (ND estatico).");
    }

    // ---- Atacantes: 40 espalhados pelos canais ----
    const uint32_t nAtt = 40;
    std::vector<uint32_t> attackerIdx; std::set<uint32_t> attackerSet;
    for (uint32_t j = 0; j < nAtt; ++j) {
        uint32_t idx = (uint32_t)std::llround((double)j * nMonitored / nAtt);
        if (idx >= nMonitored) idx = nMonitored - 1;
        if (attackerSet.insert(idx).second) attackerIdx.push_back(idx);
    }

    // ---- Sinks NO AP (recebe tudo): 9002 normal, 9001 ataque ----
    uint16_t normalPort = 9002, attackPort = 9001;
    PacketSinkHelper sinkN("ns3::UdpSocketFactory", Inet6SocketAddress(Ipv6Address::GetAny(), normalPort));
    PacketSinkHelper sinkA("ns3::UdpSocketFactory", Inet6SocketAddress(Ipv6Address::GetAny(), attackPort));
    ApplicationContainer s1 = sinkN.Install(apNode.Get(0));
    ApplicationContainer s2 = sinkA.Install(apNode.Get(0));
    s1.Start(Seconds(1.0)); s1.Stop(Seconds(900.0));
    s2.Start(Seconds(1.0)); s2.Stop(Seconds(900.0));

    // =================================================================
    //  Tráfego NORMAL: Uniforme Assíncrono (Sem Picos, Sem Phase-Lock)
    // =================================================================
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    uv->SetAttribute("Min", DoubleValue(0.0)); 
    uv->SetAttribute("Max", DoubleValue(3.0));

    for (uint32_t i = 0; i < nMonitored; ++i) {
        uint32_t k = i / nodesPerPan;
        
        OnOffHelper onoff("ns3::UdpSocketFactory", Inet6SocketAddress(apAddr[k], normalPort));
        
        // A aplicação despacha o pacote num relance (50kbps)
        onoff.SetAttribute("DataRate",   StringValue("50kbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(20)); // Carga realista
        
        onoff.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=0.001]"));
        
        // O SEGREDO: Tempo aleatório, mas com um limite mínimo rígido!
        // Min=1.0 garante que o sensor dorme PELO MENOS 1 segundo.
        // Assim, o máximo de pacotes enviados é 1 por segundo.
        // A IA não ataca nós limpos e o CSMA/CA não congestiona!
        onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"));

        ApplicationContainer app = onoff.Install(monitoredNodes.Get(i));
        // Dispersão de arranque entre os sensores (Jitter)
        app.Start(Seconds(1.0 + uv->GetValue())); 
        app.Stop(Seconds(900.0));
    }

    // ---- ATAQUE: atacante -> AP do seu canal (satura o canal local) ----
    if (attack) {
        for (uint32_t j = 0; j < attackerIdx.size(); ++j) {
            uint32_t node = attackerIdx[j];
            uint32_t k = node / nodesPerPan;
            OnOffHelper atk("ns3::UdpSocketFactory", Inet6SocketAddress(apAddr[k], attackPort));
            atk.SetAttribute("DataRate",   StringValue("128kbps"));
            atk.SetAttribute("PacketSize", UintegerValue(1000));   // fragmenta no 6LoWPAN
            atk.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=15]"));
            atk.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            ApplicationContainer app = atk.Install(monitoredNodes.Get(node));
            if (j % 2 == 0) { app.Start(Seconds(170.0 + (j/2)*0.05)); app.Stop(Seconds(220.0)); }
            else            { app.Start(Seconds(250.0 + (j/2)*0.05)); app.Stop(Seconds(300.0)); }
        }
    }

    // ---- Contadores de descarte ----
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Mac/MacTxDrop", MakeCallback(&MacTxDropCb));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Mac/MacRxDrop", MakeCallback(&MacRxDropCb));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Phy/PhyRxDrop", MakeCallback(&PhyRxDropCb));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::SixLowPanNetDevice/Drop", MakeCallback(&SixDropCb));
    // ADICIONE ESTA LINHA AQUI:
    Config::ConnectWithoutContext("/NodeList/*/$ns3::TrafficControlLayer/RootQueueDiscList/*/Drop", MakeCallback(&QueueDropCb));

    // ---- FlowMonitor + logger ----
    InstallFlowMonitor();
    Simulator::Schedule(Seconds(899.9), &SaveFlowMonXml, tag);
    g_flowCsv.open("flowmon_persec_" + tag + ".csv");
    // CABEÇALHO CORRIGIDO:
    g_flowCsv << "tempo,normal_tx_pps,normal_rx_pps,ataque_tx_pps,ataque_rx_pps\n";
    Simulator::Schedule(Seconds(1.0), &LogFlowPerSecond);

    // ---- OpenGym: a DETECCAO roda aqui, no AP central ----
    uint32_t openGymPort = 5555; double envStepTime = 1.0;
    Ptr<OpenGymInterface> openGym = CreateObject<OpenGymInterface>(openGymPort);
    openGym->SetGetObservationSpaceCb(MakeCallback(&MyGetObservationSpace));
    openGym->SetGetActionSpaceCb(MakeCallback(&MyGetActionSpace));
    openGym->SetGetObservationCb(MakeCallback(&MyGetObservation));
    openGym->SetGetRewardCb(MakeCallback(&MyGetReward));
    openGym->SetGetGameOverCb(MakeCallback(&MyGetGameOver));
    openGym->SetGetExtraInfoCb(MakeCallback(&MyGetExtraInfo));
    openGym->SetExecuteActionsCb(MakeCallback(&MyExecuteActions));
    Simulator::Schedule(Seconds(0.0), &ScheduleNextStateRead, envStepTime, openGym);

    Simulator::ScheduleDestroy(&ImprimirDescartes);
    Simulator::Stop(Seconds(901.0));
    Simulator::Run();
    g_flowCsv.close();
    Simulator::Destroy();
    return 0;
}