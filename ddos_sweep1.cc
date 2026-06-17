// =============================================================================
//  DDoS detection em ns-3 + ns3-gym, IEEE 802.15.4 (LR-WPAN) + 6LoWPAN
//  TOPOLOGIA: UM AP CENTRAL (gateway + sistema de deteccao)
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
static NodeContainer monitoredNodes;        
static uint32_t g_nNodes = 173;             
static Ptr<Node> g_ap;                      
static bool g_attack = false;               

static FlowMonitorHelper flowmonHelper;
static Ptr<FlowMonitor> flowMonitor;
static Ptr<Ipv6FlowClassifier> ipv6Classifier;
static std::map<uint32_t, uint64_t> lastTxBytesPerFlow;

static std::ofstream g_flowCsv;
static std::map<ns3::FlowId, uint64_t> g_lastTxB, g_lastRxB;

// Contadores de descarte
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
              << "Fila RAM (Overflow do Traffic Control) : " << g_queueDrop << "\n" 
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
        for (uint32_t a = 0; a < node->GetNApplications(); ++a) {
            Ptr<OnOffApplication> onoff = DynamicCast<OnOffApplication>(node->GetApplication(a));
            if (onoff) {
                if (isolate) {
                    onoff->SetAttribute("DataRate", StringValue("1bps")); 
                } else {
                    if (a == 0) {
                        onoff->SetAttribute("DataRate", StringValue("50kbps")); 
                    } else if (a == 1) {
                        onoff->SetAttribute("DataRate", StringValue("128kbps")); 
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
    uint32_t nodesPerPan = 25;       
    std::string normalRate = "200bps";
    uint32_t normalPkt = 50;
    
    // VARIÁVEIS DE CONTROLO PRINCIPAL (Podem ser alteradas via terminal)
    bool attack    = true; 
    bool useAi     = false; // <-- CHAVE MESTRA DA IA (Desligada por padrão)

    bool staticNd  = true;
    uint32_t radioQueue = 100; 
    bool tracing   = false;
    std::string tag = "apcentral";

    Config::SetDefault("ns3::Icmpv6L4Protocol::DAD", BooleanValue(false));
    
    // VACINA IPV6: Sem perda de pacotes por memória expirada
    Config::SetDefault("ns3::Icmpv6L4Protocol::ReachableTime", TimeValue(Seconds(36000.0)));
    Config::SetDefault("ns3::Icmpv6L4Protocol::RetransmissionTime", TimeValue(MilliSeconds(10)));

    CommandLine cmd(__FILE__);
    cmd.AddValue("nodesPerPan", "Dispositivos por canal 802.15.4", nodesPerPan);
    cmd.AddValue("normalRate",  "Taxa por dispositivo (ex: 200bps,100bps,67bps)", normalRate);
    cmd.AddValue("normalPkt",   "Bytes de payload por pacote normal", normalPkt);
    cmd.AddValue("attack",      "Liga o ataque DDoS", attack);
    cmd.AddValue("useAi",       "Liga o Agente Python OpenGym", useAi); // <-- Adicionado ao CMD
    cmd.AddValue("staticNd",    "Popula neighbor cache (ND estatico)", staticNd);
    cmd.AddValue("radioQueue",  "Fila do radio em pacotes (0 = default)", radioQueue);
    cmd.AddValue("tracing",     "Habilita pcap", tracing);
    cmd.AddValue("tag",         "Sufixo dos arquivos de saida", tag);
    cmd.Parse(argc, argv);

    g_attack = attack; // Passa para a variável global
    g_nNodes = nMonitored;
    const uint32_t K = (nMonitored + nodesPerPan - 1) / nodesPerPan;
    
    NS_LOG_UNCOND("==========================================================");
    NS_LOG_UNCOND("AP central com " << K << " radios (canais), " << nMonitored << " dispositivos");
    NS_LOG_UNCOND("ATAQUE DDoS LIGADO? " << (g_attack ? "SIM" : "NAO"));
    NS_LOG_UNCOND("AGENTE IA LIGADO?   " << (useAi ? "SIM" : "NAO (Rodando Nativo)"));
    NS_LOG_UNCOND("==========================================================");

    monitoredNodes.Create(nMonitored);
    NodeContainer apNode; apNode.Create(1);
    g_ap = apNode.Get(0);

    MobilityHelper apMob;
    apMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
    apPos->Add(Vector(0.0, 0.0, 0.0));
    apMob.SetPositionAllocator(apPos);
    apMob.Install(apNode);

    const double spacing = 1.0; // Espaçamento colado para eliminar o Hidden Terminal Problem
    const uint32_t cols = 5;

    std::vector<NetDeviceContainer> panSix(K);
    std::vector<uint32_t> panSliceLen(K);
    std::vector<Ptr<NetDevice>> monSix(nMonitored, nullptr);

    for (uint32_t k = 0; k < K; ++k) {
        uint32_t startIdx = k * nodesPerPan;
        uint32_t endIdx   = std::min<uint32_t>(startIdx + nodesPerPan, nMonitored);
        uint32_t sliceLen = endIdx - startIdx;
        panSliceLen[k] = sliceLen;

        NodeContainer panNodes;
        panNodes.Add(apNode.Get(0));   
        NodeContainer devSlice;
        for (uint32_t i = startIdx; i < endIdx; ++i) { 
            panNodes.Add(monitoredNodes.Get(i)); 
            devSlice.Add(monitoredNodes.Get(i)); 
        }

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

        LrWpanHelper lrwpan;
        NetDeviceContainer dev = lrwpan.Install(panNodes);
        lrwpan.CreateAssociatedPan(dev, (uint16_t)(k + 1));

        for (uint32_t di = 0; di < dev.GetN(); ++di) {
            Ptr<LrWpanNetDevice> ld = DynamicCast<LrWpanNetDevice>(dev.Get(di));
            if (!ld) continue;
            ld->GetCsmaCa()->SetMacMaxCSMABackoffs(5);  
            ld->GetMac()->SetMacMaxFrameRetries(7); 
        }

        SixLowPanHelper sixlow;
        NetDeviceContainer six = sixlow.Install(dev);
        panSix[k] = six;
        for (uint32_t li = 0; li < sliceLen; ++li)
            monSix[startIdx + li] = six.Get(li + 1);

        if (tracing)
            lrwpan.EnablePcap("ddos-" + tag + "-ch" + std::to_string(k) + "-ap", dev.Get(0), true);
    }

    InternetStackHelper stack;         
    stack.Install(apNode);
    stack.Install(monitoredNodes);

    if (radioQueue > 0) {
        TrafficControlHelper tch;
        tch.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize",
                             StringValue(std::to_string(radioQueue) + "p"));
        for (uint32_t k = 0; k < K; ++k) tch.Install(panSix[k]);
    }

    Ipv6AddressHelper address;
    std::vector<Ipv6Address> apAddr(K);   
    for (uint32_t k = 0; k < K; ++k) {
        std::ostringstream b; b << "2001:" << std::hex << (k + 1) << "::";
        address.SetBase(Ipv6Address(b.str().c_str()), Ipv6Prefix(64));
        Ipv6InterfaceContainer ifc = address.Assign(panSix[k]);
        apAddr[k] = ifc.GetAddress(0, 1);  
    }

    if (staticNd) {
        NeighborCacheHelper neighborCache;
        neighborCache.PopulateNeighborCache();
        NS_LOG_UNCOND("[INFO] Neighbor cache populada (ND estatico).");
    }

    const uint32_t nAtt = 60;
    std::vector<uint32_t> attackerIdx; std::set<uint32_t> attackerSet;
    for (uint32_t j = 0; j < nAtt; ++j) {
        uint32_t idx = (uint32_t)std::llround((double)j * nMonitored / nAtt);
        if (idx >= nMonitored) idx = nMonitored - 1;
        if (attackerSet.insert(idx).second) attackerIdx.push_back(idx);
    }

    uint16_t normalPort = 9002, attackPort = 9001;
    PacketSinkHelper sinkN("ns3::UdpSocketFactory", Inet6SocketAddress(Ipv6Address::GetAny(), normalPort));
    PacketSinkHelper sinkA("ns3::UdpSocketFactory", Inet6SocketAddress(Ipv6Address::GetAny(), attackPort));
    ApplicationContainer s1 = sinkN.Install(apNode.Get(0));
    ApplicationContainer s2 = sinkA.Install(apNode.Get(0));
    s1.Start(Seconds(1.0)); s1.Stop(Seconds(900.0));
    s2.Start(Seconds(1.0)); s2.Stop(Seconds(900.0));

    // =================================================================
    //  Tráfego NORMAL: Uniforme Assíncrono (Sem Picos)
    // =================================================================
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    uv->SetAttribute("Min", DoubleValue(0.0)); 
    uv->SetAttribute("Max", DoubleValue(3.0));

    for (uint32_t i = 0; i < nMonitored; ++i) {
        uint32_t k = i / nodesPerPan;
        OnOffHelper onoff("ns3::UdpSocketFactory", Inet6SocketAddress(apAddr[k], normalPort));
        onoff.SetAttribute("DataRate",   StringValue("50kbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(20)); 
        onoff.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=0.001]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"));

        ApplicationContainer app = onoff.Install(monitoredNodes.Get(i));
        app.Start(Seconds(1.0 + uv->GetValue())); 
        app.Stop(Seconds(900.0));
    }

    // =================================================================
    // ATAQUE DDoS (Apenas este bloco deve ser alterado)
    // =================================================================
    if (g_attack) {
        for (uint32_t j = 0; j < attackerIdx.size(); ++j) {
            uint32_t node = attackerIdx[j];
            uint32_t k = node / nodesPerPan;
            OnOffHelper atk("ns3::UdpSocketFactory", Inet6SocketAddress(apAddr[k], attackPort));
            
            // Taxa de 250kbps força a saturação do canal 802.15.4 (o limite físico)
            // Isso causará colisões e descartes APENAS durante o ataque.
            atk.SetAttribute("DataRate",   StringValue("250kbps"));
            atk.SetAttribute("PacketSize", UintegerValue(1000)); // Pacotes grandes causam maior espera no canal
            
            // Ataque mais frequente e persistente
            atk.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=20]"));
            atk.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
            
            ApplicationContainer app = atk.Install(monitoredNodes.Get(node));
            // Espalhar um pouco mais o início para criar um cenário realista de botnet
            app.Start(Seconds(170.0 + (j % 5))); 
            app.Stop(Seconds(350.0));
        }
    }

    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Mac/MacTxDrop", MakeCallback(&MacTxDropCb));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Mac/MacRxDrop", MakeCallback(&MacRxDropCb));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Phy/PhyRxDrop", MakeCallback(&PhyRxDropCb));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::SixLowPanNetDevice/Drop", MakeCallback(&SixDropCb));
    Config::ConnectWithoutContext("/NodeList/*/$ns3::TrafficControlLayer/RootQueueDiscList/*/Drop", MakeCallback(&QueueDropCb));

    InstallFlowMonitor();
    Simulator::Schedule(Seconds(899.9), &SaveFlowMonXml, tag);
    g_flowCsv.open("flowmon_persec_" + tag + ".csv");
    g_flowCsv << "tempo,normal_tx_pps,normal_rx_pps,ataque_tx_pps,ataque_rx_pps\n";
    Simulator::Schedule(Seconds(1.0), &LogFlowPerSecond);

    // =================================================================
    // MÓDULO DE INTELIGÊNCIA ARTIFICIAL (OpenGym)
    // =================================================================
    if (useAi) {
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
    } else {
        NS_LOG_UNCOND("[INFO] OpenGym Desligado. Os pacotes vao voar sem censura da IA!");
    }

    Simulator::ScheduleDestroy(&ImprimirDescartes);
    Simulator::Stop(Seconds(915.0)); 
    Simulator::Run();
    g_flowCsv.close();
    Simulator::Destroy();
    return 0;
}