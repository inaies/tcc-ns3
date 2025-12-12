// third_three_wifi_fixed_ipv6_mobility.cc
#include "ns3/opengym-module.h"   // nome exato conforme sua versão do ns3-gym
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv6-flow-classifier.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h" // Mantido por compatibilidade de includes, mas CSMA removido
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ping-helper.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ripng-helper.h"

#include <cmath>

NS_LOG_COMPONENT_DEFINE("DdosOpengym");


using namespace ns3;
static NodeContainer wifiStaNodes1; 
static NodeContainer wifiStaNodes2;
static NodeContainer wifiStaNodes3; 

static FlowMonitorHelper flowmonHelper;
static Ptr<FlowMonitor> flowMonitor;
static Ptr<Ipv6FlowClassifier> ipv6Classifier; // para mapear flowId -> endereços
static std::vector<Ptr<Node>> monitoredNodes; // nós que queremos monitorar (ex.: wifiStaNodes2)
static std::map<uint32_t, uint64_t> lastRxBytesPerFlow; // flowId -> último rxBytes
static double detectInterval = 1.0; // segundos entre verificações
static double contamination = 0.1; // fração/contaminação para anomalias (10%)


// ----------------------
// Helper: cria e instala FlowMonitor
// Chame isto antes de Simulator::Run(), após as pilhas e apps estarem instaladas.
void InstallFlowMonitor()
{
    flowMonitor = flowmonHelper.InstallAll();
    // tenta obter classifier IPv6 (se disponível)
    ipv6Classifier = DynamicCast<Ipv6FlowClassifier>(flowmonHelper.GetClassifier6());
    if (ipv6Classifier == nullptr) {
        NS_LOG_WARN("Ipv6FlowClassifier not available in this build; flow->address mapping will be unavailable.");
        // Neste caso, DetectAndMitigate terá que usar outra fonte de informação
    }
    lastRxBytesPerFlow.clear();
}

// ----------------------
// Coleta de "throughput" por source node usando FlowMonitor
// Retorna map: sourceIpv6String -> throughput (bytes/s) durante o intervalo
std::map<std::string, double> CollectNodeThroughputs(double intervalSeconds)
{
    std::map<std::string, double> nodeThroughputs;
    std::map<std::string, double> throughputBySrc;
    if (!flowMonitor) return throughputBySrc;

    flowMonitor->CheckForLostPackets();
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    for (auto &kv : stats) {
        FlowId fid = kv.first;
        FlowMonitor::FlowStats fs = kv.second;

        // mapeamento flow -> five-tuple usando classifier (se disponível)
        Ipv6FlowClassifier::FiveTuple t;

        if (ipv6Classifier == nullptr) {
            std::cout << "[WARNING] ipv6Classifier is null. Skipping throughput classification." << std::endl;
            return nodeThroughputs;
        }

        t = ipv6Classifier->FindFlow(fid);

        std::ostringstream oss;
        oss << t.sourceAddress; // operator<< formata o endereço
        std::string src = oss.str(); // string forma "2001:4::1" etc.
        uint64_t rxBytes = fs.rxBytes;

        uint64_t prev = 0;
        if (lastRxBytesPerFlow.count(fid)) prev = lastRxBytesPerFlow[fid];
        uint64_t delta = 0;
        if (rxBytes >= prev) delta = rxBytes - prev;
        lastRxBytesPerFlow[fid] = rxBytes;

        double bps = (double)delta / intervalSeconds; // bytes por segundo
        // acumula por source
        throughputBySrc[src] += bps;
    }
    return throughputBySrc;
}

// Helper to check if a node is already down to avoid redundant operations
bool IsNodeActive(Ptr<Node> node)
{
    if (!node) return false;
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    if (!ipv6) return false;
    // Check first interface (assuming index 1 is the main mesh/wifi interface)
    // Adjust index if necessary based on your topology (usually 0 is loopback, 1 is wifi)
    if (ipv6->GetNInterfaces() > 1) {
        return ipv6->IsUp(1); 
    }
    return false;
}

/* --------------------------
 *  OpenGym Interface Callbacks
 * -------------------------- */

Ptr<OpenGymSpace> MyGetObservationSpace(void)
{
  uint32_t nodeNum = 10;
  float low = 0.0;
  float high = 1e9; // 1 Gbps max
  std::vector<uint32_t> shape = {nodeNum};
  std::string dtype = TypeNameGet<float>();
  Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace>(low, high, shape, dtype);
  return space;
}

Ptr<OpenGymSpace> MyGetActionSpace(void)
{
    uint32_t N = 10; 
    std::vector<uint32_t> shape = {N};
    std::vector<float> low(N, 0.0f);
    std::vector<float> high(N, 1.0f);
    Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace>(low, high, shape, "float32");
    return space;
}

Ptr<OpenGymDataContainer> MyGetObservation(void)
{
  uint32_t nodeNum = 10;
  std::vector<uint32_t> shape = {nodeNum};
  Ptr<OpenGymBoxContainer<float>> box = CreateObject<OpenGymBoxContainer<float>>(shape);

  // 1. Coleta dados reais de throughput
  std::map<std::string, double> tpMap = CollectNodeThroughputs(1.0);

  // 2. Mapeia para os nós monitorados
  for (uint32_t i = 0; i < nodeNum && i < wifiStaNodes2.GetN(); i++)
  {
    float val = 0.0f;
    Ptr<Node> node = wifiStaNodes2.Get(i);
    
    if (node) {
        Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
        if (ipv6) {
            for(uint32_t ifIdx=0; ifIdx < ipv6->GetNInterfaces(); ++ifIdx) {
                 // Segurança: garante que tem endereço global antes de acessar index 1
                 if (ipv6->GetNAddresses(ifIdx) < 2) continue;

                 Ipv6Address addr = ipv6->GetAddress(ifIdx, 1).GetAddress();
                 std::ostringstream oss;
                 oss << addr;
                 
                 if (tpMap.count(oss.str())) {
                     val = (float)tpMap[oss.str()];
                 }
            }
        }
    }
    box->AddValue(val);
  }

  // --- LOG VISUAL ---
  std::vector<float> data = box->GetData();
  std::stringstream ss;
  ss << "[";
  for (size_t i = 0; i < data.size(); ++i) {
      ss << data[i];
      if (i < data.size() - 1) ss << ", ";
  }
  ss << "]";

  // Imprime o tempo atual e o vetor de dados
  NS_LOG_UNCOND("MyGetObservation [Time: " << Simulator::Now().GetSeconds() << "s]: " << ss.str());

  return box;
}

float MyGetReward(void)
{
  return 1.0;
}

bool MyGetGameOver(void)
{
  return Now().GetSeconds() >= 100.0;
}

std::string MyGetExtraInfo(void)
{
  std::stringstream ss;
  
  for (uint32_t i = 0; i < wifiStaNodes2.GetN(); ++i) {
      Ptr<Node> node = wifiStaNodes2.Get(i);
      if (!node) continue;
      
      Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
      if (!ipv6) continue;

      // Busca o endereço global (pula o loopback e link-local)
      for(uint32_t ifIdx=0; ifIdx < ipv6->GetNInterfaces(); ++ifIdx) {
           if (ipv6->GetNAddresses(ifIdx) < 2) continue; 

           Ipv6Address addr = ipv6->GetAddress(ifIdx, 1).GetAddress();
           
           // Formato: INDICE=IP|
           ss << i << "=" << addr << "|";
           break; // Pega o primeiro IP global e vai pro próximo nó
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

    for (uint32_t i = 0; i < actions.size() && i < wifiStaNodes2.GetN(); ++i) {
        if (actions[i] > 0.5f) {
            Ptr<Node> node = wifiStaNodes2.Get(i);
            if (!node) continue;

            // Tenta pegar o IP apenas para o log ficar bonito
            std::string nodeIp = "Unknown";
            Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
            if (ipv6) {
                for(uint32_t k=0; k<ipv6->GetNInterfaces(); ++k) {
                    if (ipv6->GetNAddresses(k) >= 2) {
                        std::ostringstream oss;
                        oss << ipv6->GetAddress(k, 1).GetAddress();
                        nodeIp = oss.str();
                        break;
                    }
                }
            }

            // Parar Aplicação OnOff
            bool appStopped = false;
            for (uint32_t j = 0; j < node->GetNApplications(); ++j) {
                Ptr<Application> app = node->GetApplication(j);
                if (app->GetInstanceTypeId() == OnOffApplication::GetTypeId()) {
                    app->SetStopTime(Simulator::Now()); 
                    appStopped = true;
                }
            }
            
            if (appStopped) {
                NS_LOG_UNCOND(">>> [ACTION] Agente ISOLOU Node " << i << " (IP: " << nodeIp << ")");
            }
        }
    }
    return true;
}

void ScheduleNextStateRead(double envStepTime, Ptr<OpenGymInterface> openGym)
{
  openGym->NotifyCurrentState();
  Simulator::Schedule(Seconds(envStepTime), &ScheduleNextStateRead, envStepTime, openGym);
}

// ----------------------
// Detecta anomalias por z-score sobre a taxa agregada por fonte.
// Retorna set de endereços IPv6 (strings) considerados anômalos.
std::set<std::string> DetectAnomalousSources(const std::map<std::string,double> &throughputMap)
{
    std::set<std::string> anomalies;
    if (throughputMap.empty()) return anomalies;

    // extrai valores
    std::vector<double> vals;
    vals.reserve(throughputMap.size());
    for (auto &kv : throughputMap) vals.push_back(kv.second);

    double mean = 0.0;
    for (double v : vals) mean += v;
    mean /= (double)vals.size();

    double var = 0.0;
    for (double v : vals) var += (v - mean)*(v - mean);
    var /= (double)vals.size();
    double stdv = sqrt(std::max(var, 1e-9));

    // calcula score (|z|) por fonte
    std::vector<std::pair<std::string,double>> scored;
    for (auto &kv : throughputMap) {
        double z = fabs((kv.second - mean) / stdv);
        scored.emplace_back(kv.first, z);
    }
    // ordena por score decrescente
    sort(scored.begin(), scored.end(), [](auto &a, auto &b){ return a.second > b.second; });

    // top-k por contamination
    int n = (int)scored.size();
    int k = std::max(1, (int)round(contamination * n));
    for (int i = 0; i < k && i < n; ++i) {
        anomalies.insert(scored[i].first);
    }
    return anomalies;
}

void ShutDownNode(Ptr<Node> node)
{
    if (node == nullptr)
    {
        NS_LOG_WARN("Attempted to shut down null node.");
        return;
    }

    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    if (ipv6 == nullptr)
    {
        NS_LOG_WARN("Node " << node->GetId() << " has no IPv6 stack, cannot shut down.");
        return;
    }

    for (uint32_t ifIndex = 0; ifIndex < ipv6->GetNInterfaces(); ++ifIndex)
    {
        ipv6->SetDown(ifIndex);

        // Simulator::Schedule(Seconds(5.0), &Ipv6::SetDown, ipv6, ifIndex);
    }

    NS_LOG_INFO("Node " << node->GetId() << " shutdown (interfaces down).");
}

// ----------------------
// Aplica mitigação: isola nós que correspondem aos endereços anômalos.
// Estratégia: para cada address anômalo, procura o Node que tem aquela interface
// IPv6 e chama ipv6->SetDown(ifIndex) para "tirar a interface do ar" (isolar tráfego).
// OBS: este método exige que você saiba quais containers/devices correspondem a cada nó.
// Passe aqui os containers (ex.: wifiStaNodes2 e staDevices2) ou ajuste a busca.
void IsolateSourcesByAddress(const std::set<std::string> &anomalousSources,
                             NodeContainer &nodes,
                             NetDeviceContainer &devices)
{
    for (auto &s : anomalousSources)
    {
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Node> node = nodes.Get(i);
            if (node == nullptr)
            {
                NS_LOG_WARN("Skipping null node pointer.");
                continue;
            }

            Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
            if (ipv6 == nullptr)
            {
                NS_LOG_WARN("Node " << node->GetId() << " has no IPv6 interface, skipping.");
                continue;
            }

            for (uint32_t ifIndex = 0; ifIndex < ipv6->GetNInterfaces(); ++ifIndex)
            {
                if (ipv6->GetNAddresses(ifIndex) == 0)
                    continue;

                Ipv6InterfaceAddress ifAddr = ipv6->GetAddress(ifIndex, 0);
                if (ifAddr.GetAddress().IsAny())
                    continue;

                Ipv6Address addr = ifAddr.GetAddress();
                if (addr == Ipv6Address(s.c_str()))
                {
                    NS_LOG_INFO("Isolating node " << node->GetId()
                                 << " with address " << s
                                 << " (ifIndex " << ifIndex << ")");
                    ShutDownNode(node);
                }
            }
        }
    }
}

// ----------------------
// Função agendada: executa coleta, detecção e mitigação periódica
// Chamar a primeira vez antes do Simulator::Run, por exemplo:
// Simulator::Schedule(Seconds( detectInterval ), &DetectAndMitigate, detectInterval, wifiStaNodes2, staDevices2);
void DetectAndMitigate(double intervalSeconds, NodeContainer allStaNodes, NetDeviceContainer allStaDevices)
{
    // 1) coleta
    auto tp = CollectNodeThroughputs(intervalSeconds);

    // 2) detectar anomalias
    auto anoms = DetectAnomalousSources(tp);
    if (!anoms.empty()) {
        NS_LOG_INFO("Anomalous sources detected: ");
        for (auto &a : anoms) NS_LOG_INFO("  " << a);
    } else {
        NS_LOG_INFO("No anomalies detected in this interval.");
    }

    // 3) aplicar mitigação (isolation) — exemplo: apenas nos STAs do segundo AP (wifiStaNodes2)
    IsolateSourcesByAddress(anoms, allStaNodes, allStaDevices);

    // 4) reagendar próxima verificação
    Simulator::Schedule(Seconds(intervalSeconds), &DetectAndMitigate, intervalSeconds, allStaNodes, allStaDevices);
}

class DdosEnv : public Object {
public:
  DdosEnv(Ptr<Node> apNode, std::vector<Ptr<Node>> staNodes /*...*/) {
    m_openGym = CreateObject<OpenGymInterface>();
    // registo callbacks — sintaxe depende da versão; exemplo conceitual:
    m_openGym->SetGetObservationSpaceCb(MakeCallback(&DdosEnv::GetObservationSpace, this));
    // similarly for others...
  }

  Ptr<OpenGymSpace> GetObservationSpace() { return nullptr; }
  Ptr<OpenGymSpace> GetActionSpace() { return nullptr; }
  Ptr<OpenGymDataContainer> GetObservation() { return nullptr; }
  float GetReward() { return 0.0f; }
  bool GetGameOver() { return false; }
  std::string GetExtraInfo() { return std::string("info"); }
  bool ExecuteActions(Ptr<OpenGymDataContainer> action) {
      // converter 'action' para operações ns-3:
      // - isolar nó -> desabilitar NetDevice do STA ou SetDown interface IPv6
      // - promover deputy -> alterar variáveis internas do sim (ex.: marcar outro AP como leader)
      // - reforçar malha -> adicionar rota alternativa/reativar link
      return true;
  }
private:
  Ptr<OpenGymInterface> m_openGym;
  // referência para nós/containers do seu cenário
};


static Ptr<ListPositionAllocator>
CreateGridPositionAllocator (uint32_t nNodes, double spacing, double offsetX, double offsetY)
{
  Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator> ();
  if (nNodes == 0)
    return allocator;
  uint32_t cols = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(nNodes))));
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      uint32_t row = i / cols;
      uint32_t col = i % cols;
      double x = offsetX + col * spacing;
      double y = offsetY + row * spacing;
      allocator->Add (Vector (x, y, 0.0));
    }
  return allocator;
}

int
main(int argc, char* argv[])
{
    LogComponentEnable("Ping", LOG_LEVEL_INFO);
    LogComponentEnable("DdosOpengym", LOG_LEVEL_INFO);

    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    bool verbose = true;
    uint32_t nWifiCsma = 173; // nCsma renomeado para nWifiCsma
    uint32_t nWifi = 173;
    bool tracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    if (nWifi > 200) // segurança para grids gigantes
    {
        std::cout << "nWifi muito grande; ajuste o script ou aumente a área." << std::endl;
        return 1;
    }

    wifiStaNodes1.Create(nWifi);
    wifiStaNodes2.Create(nWifi);
    wifiStaNodes3.Create(nWifi);

    NodeContainer p2pNodes;
    p2pNodes.Create(3); // n0=AP1, n1=AP2/WiFi3 AP, n2=AP3

    // Ponto-a-Ponto
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ap1ap2, ap1ap3, ap2ap3;
    ap1ap2 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(1));
    ap1ap3 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));
    ap2ap3 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));

    // --------------------------------------------------------------------------------
    // WiFi nodes
    // --------------------------------------------------------------------------------

    NodeContainer wifiApNode  = p2pNodes.Get(0); // AP1
    NodeContainer wifiApNode2 = p2pNodes.Get(1); // AP2
    NodeContainer wifiApNode3 = p2pNodes.Get(2); // AP3 (WiFi3)

    // PHY/MAC (idem ao original)
    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy1; phy1.SetChannel(channel1.Create());
    phy1.Set("ChannelSettings", StringValue("{36, 0, BAND_5GHZ, 0}"));

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy2; phy2.SetChannel(channel2.Create());
    phy2.Set("ChannelSettings", StringValue("{40, 0, BAND_5GHZ, 0}"));

    YansWifiChannelHelper channel3 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy3; phy3.SetChannel(channel3.Create());
    phy3.Set("ChannelSettings", StringValue("{44, 0, BAND_5GHZ, 0}"));

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    // wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");

    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");
    Ssid ssid3 = Ssid("ns-3-ssid-3");

    // WiFi 1 (AP1)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy1, mac, wifiStaNodes1);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifi.Install(phy1, mac, wifiApNode);

    // WiFi 2 (AP3)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi.Install(phy2, mac, wifiStaNodes2);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevices2 = wifi.Install(phy2, mac, wifiApNode2);

    // WiFi 3 (AP2)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices3 = wifi.Install(phy3, mac, wifiStaNodes3);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    NetDeviceContainer apDevices3 = wifi.Install(phy3, mac, wifiApNode3);

    // Config::SetDefault(
    //     "ns3::WifiMacQueue::MaxSize",
    //     QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, std::numeric_limits<uint32_t>::max())));
    // Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(Seconds(10)));

    // --------------------------------------------------------------------------------
    // Mobilidade ADAPTADA para aumentar capacidade (isolar células e controlar densidade)
    // --------------------------------------------------------------------------------

    MobilityHelper mobility;

    // Parâmetros: espaçamento entre nós na grade e offsets para separar redes
    double spacing = 5.0;    // distância entre STAs (m). Ajuste para maior densidade se quiser mais nós por área.
    double offsetCell = 75.0; // distância entre centros das células -> isola co-canal interference

    // Cria alocadores de posição separados para cada rede (mantém as redes fisicamente separadas)
    Ptr<ListPositionAllocator> allocWifi1 = CreateGridPositionAllocator (nWifi, spacing, 0.0, 0.0);
    Ptr<ListPositionAllocator> allocWifi2 = CreateGridPositionAllocator (nWifi, spacing, 0.0, offsetCell);    // deslocada em Y
    Ptr<ListPositionAllocator> allocWifi3 = CreateGridPositionAllocator (nWifiCsma, spacing, offsetCell, 0.0);  // deslocada em X

    // Instala posições e modelo constante nos STAs (posições fixas na grade)
    mobility.SetPositionAllocator (allocWifi1);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes1);

    mobility.SetPositionAllocator (allocWifi2);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes2);

    mobility.SetPositionAllocator (allocWifi3);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes3);

    // Coloca APs perto do centro de cada grade
    Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator> ();

    // Centro aproximado de cada grade (calcula colunas a partir do número de nós)
    uint32_t cols1 = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(std::max<uint32_t>(1, nWifi)))));
    uint32_t cols3 = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(std::max<uint32_t>(1, nWifiCsma)))));

    // AP1 center
    double ap1x = (cols1 * spacing) / 2.0;
    double ap1y = (cols1 * spacing) / 2.0;
    apAlloc->Add (Vector (ap1x, ap1y, 0.0));

    // AP2 (WiFi2) center - offset in Y
    double ap2x = (cols1 * spacing) / 2.0;
    double ap2y = offsetCell + (cols1 * spacing) / 2.0;
    apAlloc->Add (Vector (ap2x, ap2y, 0.0));
    
    // AP3 (for WiFi3) center - note: this AP is at offsetCell in X
    double ap3x = offsetCell + (cols3 * spacing) / 2.0;
    double ap3y = (cols3 * spacing) / 2.0;
    apAlloc->Add (Vector (ap3x, ap3y, 0.0));

    mobility.SetPositionAllocator (apAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

    // Note: Install on AP NodeContainers individually so each AP receives corresponding position
    mobility.Install (wifiApNode);  // AP1 -> first position
    mobility.Install (wifiApNode2); // AP2 -> third position
    mobility.Install (wifiApNode3); // AP3 -> second position

    // --------------------------------------------------------------------------------
    // [RESTA DO CÓDIGO: pilhas, endereçamento, roteamento, aplicações...]
    // --------------------------------------------------------------------------------

    // 1. Roteadores (n0, n1, n2) usam RIPng
    RipNgHelper ripNg;
    Ipv6ListRoutingHelper listRh;
    listRh.Add(ripNg, 0);

    InternetStackHelper routerStack;
    routerStack.SetRoutingHelper(listRh);
    routerStack.Install(p2pNodes); // n0, n1, n2

    // 2. Nós Finais (STAs das três redes) usam Ipv6StaticRouting
    Ipv6StaticRoutingHelper ipv6StaticRouting;
    InternetStackHelper staStack;
    staStack.SetRoutingHelper(ipv6StaticRouting);

    staStack.Install(wifiStaNodes1);
    staStack.Install(wifiStaNodes2);
    staStack.Install(wifiStaNodes3); // Instalar nos novos STAs

    // Endereçamento IPv6 (mesma lógica do seu original)
    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64)); // AP1-AP2
    Ipv6InterfaceContainer ap1ap2Interfaces = address.Assign(ap1ap2);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64)); // WiFi1 (AP1)
    Ipv6InterfaceContainer wifiInterfaces1 = address.Assign(staDevices1);
    Ipv6InterfaceContainer apInterfaces1   = address.Assign(apDevices1);

    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64)); 
    Ipv6InterfaceContainer wifiInterfaces2 = address.Assign(staDevices2);
    Ipv6InterfaceContainer apInterfaces2   = address.Assign(apDevices2);

    address.SetBase(Ipv6Address("2001:7::"), Ipv6Prefix(64)); // NOVO: WiFi3 (AP2)
    Ipv6InterfaceContainer wifiInterfaces3 = address.Assign(staDevices3);
    Ipv6InterfaceContainer apInterfaces3   = address.Assign(apDevices3);

    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64)); // AP1-AP3
    Ipv6InterfaceContainer ap1ap3Interfaces = address.Assign(ap1ap3);

    address.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64)); // AP2-AP3
    Ipv6InterfaceContainer ap2ap3Interfaces = address.Assign(ap2ap3);

    // Habilitar Forwarding (Roteamento) nos Roteadores (p2pNodes)
    for (uint32_t i = 0; i < p2pNodes.GetN(); ++i)
    {
        Ptr<Ipv6> ipv6 = p2pNodes.Get(i)->GetObject<Ipv6>();
        ipv6->SetForwarding(0, true);
    }

    // Rotas estáticas nos STAs (idêntico ao seu original)
    Ipv6Address ap1Addr = apInterfaces1.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes1.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes1.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices1.Get(i));
        sr->SetDefaultRoute(ap1Addr, ifSta);
    }

    Ipv6Address ap2Addr = apInterfaces2.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes2.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes2.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices2.Get(i));
        sr->SetDefaultRoute(ap2Addr, ifSta);
    }

    Ipv6Address ap3Addr = apInterfaces3.GetAddress(0, 1);
    for (uint32_t i = 0; i < wifiStaNodes3.GetN(); i++)
    {
        Ptr<Ipv6> ipv6 = wifiStaNodes3.Get(i)->GetObject<Ipv6>();
        Ptr<Ipv6StaticRouting> sr = ipv6StaticRouting.GetStaticRouting(ipv6);
        uint32_t ifSta = ipv6->GetInterfaceForDevice(staDevices3.Get(i));
        sr->SetDefaultRoute(ap3Addr, ifSta);
    }

    // 1. Configuração do Receptor (Sink) no AP2 (n1)
    Ptr<Node> ap2_receptor = wifiApNode2.Get(0); // AP2 (n1)
    uint16_t sinkPort = 9002;
    
    PacketSinkHelper sinkHelper(
      "ns3::UdpSocketFactory",
      Inet6SocketAddress(Ipv6Address::GetAny(), sinkPort)
    );
    ApplicationContainer sinkApp = sinkHelper.Install(ap2_receptor);
    sinkApp.Start(Seconds(1.5)); // Começa cedo
    sinkApp.Stop(Seconds(100.0)); // Para cedo

    // 2. Configuração do Emissor (OnOff)
    
    // O AP2 está na rede 2001:4::/64. O AP2 é o sink.
    Ipv6Address ap2_address = apInterfaces2.GetAddress(0, 1); 

    OnOffHelper onoff("ns3::UdpSocketFactory",
        Address(Inet6SocketAddress(ap2_address, sinkPort)));
    
    // Taxa baixa para garantir que o AP possa receber (ex: 100kbps)
    onoff.SetAttribute("DataRate", StringValue("100kbps"));
    // Envia apenas UM pacote por intervalo, para garantir que o AP consiga processar
    onoff.SetAttribute("PacketSize", UintegerValue(1000)); // Tamanho do pacote em bytes
    // O "OnTime" será o tempo de transmissão de um único pacote (muito curto)
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); 
    // O "OffTime" deve ser um tempo grande para o nó não repetir o envio
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    // 3. Agendamento Sequencial
    double start_offset = 10.0; // Tempo inicial de start
    double interval = 2;     // Intervalo entre o start de cada nó (50ms)
    
    // Apenas nos nós da Rede 2 (wifiStaNodes2)
    for (uint32_t i = 21; i < wifiStaNodes2.GetN(); i++)
    {
      // Cria uma instância do OnOffHelper para cada nó
      ApplicationContainer clientApp = onoff.Install(wifiStaNodes2.Get(i));
      
      // Agenda o início da transmissão do nó 'i'
      clientApp.Start(Seconds(start_offset + (i-21) * interval));
      clientApp.Stop(Seconds(start_offset + (i-21) * interval + 1.0)); // Roda por 1 segundo apenas
    }

/* ///////////   ataque ddos   ////////// */

    NodeContainer attackerNodes;
    for (int i = 0; i < 20; i ++)
      attackerNodes.Add(wifiStaNodes2.Get(i));

    Ptr<Node> victim = wifiApNode2.Get(0);
    
    Ipv6Address victimAddress = apInterfaces2.GetAddress(0, 1);

    uint16_t attackPort = 9001;
    
    PacketSinkHelper udpSinkHelper(
      "ns3::UdpSocketFactory",
      Inet6SocketAddress(Ipv6Address::GetAny(), attackPort)
    );
    ApplicationContainer sinkAppAttack = udpSinkHelper.Install(victim);
    sinkAppAttack.Start(Seconds(1.0));
    sinkAppAttack.Stop(Seconds(110.0));
  
    for (uint32_t i = 0; i < attackerNodes.GetN(); i++)
    {
      OnOffHelper onoff(
        "ns3::UdpSocketFactory",
        Address(Inet6SocketAddress(victimAddress, attackPort))
      );
      onoff.SetAttribute("DataRate", StringValue("5Mbps"));
      onoff.SetAttribute("PacketSize", UintegerValue(1024));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=15]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

      ApplicationContainer attackApp = onoff.Install(attackerNodes);
      attackApp.Start(Seconds(30.0));
      attackApp.Stop(Seconds(90.0));
    }

    InstallFlowMonitor();

    // Simulator::Schedule(Seconds(detectInterval), &DetectAndMitigate, detectInterval, wifiStaNodes2, staDevices2);
  
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

    // Inicia loop Gym
    Simulator::Schedule(Seconds(0.0), &ScheduleNextStateRead, envStepTime, openGym);
    
    if (tracing)
    {
        pointToPoint.EnablePcapAll("p2p-traffic-ddos");

        phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy2.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy3.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

        phy1.EnablePcap("ddosml_ap1", apDevices1.Get(0)); // AP1
        phy2.EnablePcap("ddosml_ap2", apDevices2.Get(0)); // AP1
        phy3.EnablePcap("ddosml_ap3", apDevices3.Get(0)); // AP1
    }
    
    Simulator::Stop(Seconds(101.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}