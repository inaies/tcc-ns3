#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <random>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <tuple>
#include <set>

// -------------------------------------
// 1. Definições de Estruturas (Substituindo Dicionários e Atributos de Nós/Arestas)
// -------------------------------------

// Estrutura para atributos de um nó
struct NodeAttrs {
    double traffic = 0.0;
    double packet_rate = 0.0;
    double latency = 0.0;
    double energy = 0.0;
    std::string role = "member";
    bool authenticated = false;
    bool alive = true;
    int hello_count = 0; // Para a fase de formação
};

// Estrutura para atributos de uma aresta
struct EdgeAttrs {
    double quality = 0.0;
    double capacity = 0.0;
};

// Estrutura de Grafo (Placeholder para Boost Graph Library, etc.)
// Usamos mapas e listas de adjacência simples para este esboço
using NodeID = int;
using AdjacencyList = std::map<NodeID, std::vector<NodeID>>;
using NodeData = std::map<NodeID, NodeAttrs>;
using EdgeData = std::map<std::pair<NodeID, NodeID>, EdgeAttrs>;

struct Graph {
    AdjacencyList adj;
    NodeData node_data;
    EdgeData edge_data;

    int num_nodes() const { return node_data.size(); }
    int num_edges() const { return edge_data.size(); }
    std::vector<NodeID> get_neighbors(NodeID v) const {
        auto it = adj.find(v);
        return (it != adj.end()) ? it->second : std::vector<NodeID>();
    }
    bool has_edge(NodeID u, NodeID v) const {
        if (u > v) std::swap(u, v);
        return edge_data.count({u, v});
    }
    void remove_edge(NodeID u, NodeID v); // Implementação fora da struct
    void add_edge(NodeID u, NodeID v, double q, double c); // Implementação fora da struct
};

// Estruturas de Configuração
using Config = std::map<std::string, double>;
using AttkConfig = std::map<std::string, double>;

// Q-valor (Memória Q-Learning)
using QValues = std::map<std::string, double>;
QValues fresh_Q() {
    return {
        {"OK", 0.0},
        {"REINFORCE_MESH", 0.0},
        {"ISOLATE_SUSPECTS_AND_REWIRE", 0.0},
        {"PROMOTE_DEPUTY_AND_REWIRE", 0.0}
    };
}

// Configurações Globais
const int RANDOM_SEED = 42;
const int N_NODES = 50;
const int N_CYCLES = 25;
const AttkConfig ATTACK_CFG = {
    {"num_node_failures", 4.0},
    {"num_link_failures", 6.0},
    {"num_dos_nodes", 5.0},
    {"attack_leader", 1.0} // 1.0 para true
};

// Gerador de números aleatórios global
std::mt19937 rng(RANDOM_SEED);

// -------------------------------------
// 2. Placeholder para Machine Learning (ML)
// -------------------------------------

// Placeholder para dados de features (em C++ real, seria Eigen::Matrix, etc.)
using FeatureMatrix = std::vector<std::vector<double>>;
using Predictions = std::vector<int>;

class IsolationForest {
public:
    IsolationForest(double contamination, int seed) {}
    void fit(const FeatureMatrix& X) {}
    Predictions predict(const FeatureMatrix& X) const {
        // Simulação: Retorna -1 (outlier) para alguns
        Predictions p(X.size());
        std::uniform_real_distribution<> dist(0.0, 1.0);
        for (size_t i = 0; i < X.size(); ++i) {
            p[i] = (dist(rng) < 0.15) ? -1 : 1;
        }
        return p;
    }
};

class LocalOutlierFactor {
public:
    LocalOutlierFactor(int n_neighbors, double contamination) {}
    Predictions fit_predict(const FeatureMatrix& X) const {
        // Simulação: Retorna -1 (outlier) para alguns
        Predictions p(X.size());
        std::uniform_real_distribution<> dist(0.0, 1.0);
        for (size_t i = 0; i < X.size(); ++i) {
            p[i] = (dist(rng) < 0.10) ? -1 : 1;
        }
        return p;
    }
};

// -------------------------------------
// 3. Funções Utilitárias e Adaptações de Grafos
// -------------------------------------

// Funções de Graph (Implementação simplificada)
void Graph::remove_edge(NodeID u, NodeID v) {
    if (u > v) std::swap(u, v);
    edge_data.erase({u, v});

    // Remove da lista de adjacência
    auto remove_from_adj = [&](NodeID a, NodeID b) {
        auto& neighbors = adj[a];
        neighbors.erase(std::remove(neighbors.begin(), neighbors.end(), b), neighbors.end());
    };
    remove_from_adj(u, v);
    remove_from_adj(v, u);
}

void Graph::add_edge(NodeID u, NodeID v, double q, double c) {
    if (u == v) return;
    NodeID a = std::min(u, v);
    NodeID b = std::max(u, v);

    if (!edge_data.count({a, b})) {
        edge_data[{a, b}] = {q, c};
        adj[u].push_back(v);
        adj[v].push_back(u);
    }
}

// Placeholder para construção de topologias (simula a criação de nós)
Graph build_topology(const std::string& kind, int n, int seed) {
    Graph G;
    rng.seed(seed); // Usa a seed para a topologia
    std::uniform_real_distribution<> u_traffic(0.1, 1.0);
    std::uniform_real_distribution<> u_rate(1, 50);
    std::uniform_real_distribution<> u_latency(5, 50);
    std::uniform_real_distribution<> u_energy(0.4, 1.0);
    std::uniform_real_distribution<> u_quality(0.7, 1.0);
    std::uniform_real_distribution<> u_capacity(1.0, 10.0);

    for (int i = 0; i < n; ++i) {
        G.node_data[i] = {
            u_traffic(rng), u_rate(rng), u_latency(rng), u_energy(rng),
            "member", false, true, 0
        };
    }

    // A lógica de criação de ER/WS/BA seria feita aqui,
    // tipicamente usando BGL ou similar, e adicionando arestas com add_edge.
    // Exemplo Simples (Apenas para demonstração, não é ER/WS/BA real):
    if (kind == "ER") {
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                if (std::uniform_real_distribution<>(0.0, 1.0)(rng) < 0.10) {
                    G.add_edge(i, j, u_quality(rng), u_capacity(rng));
                }
            }
        }
    }

    return G;
}

// Placeholder para componente gigante, eficiência de propagação, etc.
// Em C++ real, isso usaria algoritmos BGL (BFS, Dijkstra, Componentes Conexas).
double largest_component_fraction(const Graph& G) {
    // Simulação: 90%
    return 0.90;
}
double propagation_efficiency(const Graph& G) {
    // Simulação: 0.15
    return 0.15;
}
double delivery_ratio_proxy(const Graph& G) {
    // Simulação: 0.50
    return 0.50;
}

// -------------------------------------
// 4. Os 11 Passos Operacionais (Adaptados)
// -------------------------------------

void formation_phase(Graph& G) {
    std::uniform_int_distribution<> u_hello(3, 10);
    for (auto& pair : G.node_data) {
        pair.second.hello_count = u_hello(rng);
    }
}

std::pair<NodeID, NodeID> elect_leaders(Graph& G) {
    // Calcula grau e ordena. Em BGL, o grau seria mais eficiente.
    std::vector<std::pair<int, NodeID>> degrees;
    for (const auto& pair : G.node_data) {
        degrees.push_back({G.get_neighbors(pair.first).size(), pair.first});
        pair.second.role = "member";
    }
    std::sort(degrees.rbegin(), degrees.rend());

    NodeID leader = degrees[0].second;
    NodeID deputy = degrees[1].second;
    G.node_data[leader].role = "leader";
    G.node_data[deputy].role = "deputy";

    return {leader, deputy};
}

bool authenticate_leaders(Graph& G, NodeID leader, NodeID deputy) {
    bool okL = (G.node_data[leader].energy > 0.5) && (G.node_data[leader].latency < 40);
    bool okD = (G.node_data[deputy].energy > 0.5) && (G.node_data[deputy].latency < 40);
    G.node_data[leader].authenticated = okL;
    G.node_data[deputy].authenticated = okD;
    return okL && okD;
}

void operation_step(Graph& G) {
    std::normal_distribution<> n_load(1.0, 0.1);
    std::uniform_real_distribution<> u_qual(0.0, 0.01);

    for (auto& pair : G.node_data) {
        NodeAttrs& v = pair.second;
        if (!v.alive) continue;
        double load = std::clamp(n_load(rng), 0.5, 1.5);

        v.traffic = std::clamp(v.traffic * load, 0.05, 5.0);
        v.packet_rate = std::clamp(v.packet_rate * load, 0.5, 200.0);
        v.latency = std::clamp(v.latency * (2 - load), 2.0, 200.0);
        v.energy = std::max(0.0, v.energy - 0.005 * load);
    }
    for (auto& pair : G.edge_data) {
        EdgeAttrs& e = pair.second;
        e.quality = std::clamp(e.quality - u_qual(rng), 0.2, 1.0);
    }
}

std::set<NodeID> inject_failures_and_attacks(Graph& G, NodeID leader, const AttkConfig& cfg) {
    // Lógica para falhas de nós/links e DoS (Adaptado do Python)
    std::set<NodeID> ground_truth;
    // ... (Implementação omitida por brevidade, mas segue a lógica Python)
    return ground_truth;
}

// -------------------------------------
// 5. Funções de Metricas e Q-Value
// -------------------------------------

std::tuple<double, std::map<std::string, double>> quality_index(const Graph& G) {
    double c = largest_component_fraction(G);
    double p = propagation_efficiency(G);
    double d = delivery_ratio_proxy(G);
    double qi = 0.5 * c + 0.3 * p + 0.2 * d;
    std::map<std::string, double> parts = {
        {"connectivity_frac", c},
        {"prop_eff", p},
        {"delivery_ratio", d}
    };
    return {qi, parts};
}

std::string action_from_quality(double qi) {
    if (qi >= 0.75) return "OK";
    if (qi >= 0.55) return "REINFORCE_MESH";
    if (qi >= 0.35) return "ISOLATE_SUSPECTS_AND_REWIRE";
    return "PROMOTE_DEPUTY_AND_REWIRE";
}

void update_Q(QValues& Q, const std::string& action, double qi_before, double qi_after, double del_before, double del_after) {
    double reward = (qi_after - qi_before) + 0.5 * (del_after - del_before);
    Q[action] = 0.8 * Q.at(action) + 0.2 * reward;
}

// -------------------------------------
// 6. Detecção e Resiliência
// -------------------------------------

std::pair<std::set<NodeID>, std::map<NodeID, double>> detect_anomalies(
    const Graph& G,
    const std::map<NodeID, std::tuple<double, double, double>>& pre_snapshot,
    const std::set<NodeID>& ground_truth,
    const Config& cfg) {
    
    std::vector<NodeID> alive_nodes;
    for(const auto& pair : G.node_data) {
        if (pair.second.alive) alive_nodes.push_back(pair.first);
    }
    if (alive_nodes.empty()) return {{}, {}};

    FeatureMatrix X;
    // Lógica para construir a FeatureMatrix X (adaptando os 8 recursos do Python)
    // ... (Omitida por brevidade)

    // Contaminação
    double cont = cfg.at("base_contamination");
    if (!ground_truth.empty()) {
        cont = std::clamp(1.2 * ground_truth.size() / std::max(2, (int)alive_nodes.size()), 0.02, 0.25);
    }
    
    // ML: IF
    IsolationForest clf_if(cont, RANDOM_SEED);
    clf_if.fit(X);
    Predictions pred_if = clf_if.predict(X);

    Predictions y_vote = pred_if;
    if (cfg.at("use_lof_ensemble") == 1.0) {
        // ML: LOF
        LocalOutlierFactor lof(20, cont);
        Predictions pred_lof = lof.fit_predict(X);
        
        // Votação (adaptação do OR lógico)
        for(size_t i = 0; i < X.size(); ++i) {
            if (pred_if[i] == -1 || pred_lof[i] == -1) {
                y_vote[i] = -1;
            }
        }
    }

    std::set<NodeID> anomalies;
    std::map<NodeID, double> scores;
    for(size_t i = 0; i < y_vote.size(); ++i) {
        scores[alive_nodes[i]] = y_vote[i];
        if (y_vote[i] == -1) {
            anomalies.insert(alive_nodes[i]);
        }
    }
    return {anomalies, scores};
}

// ... (Funções apply_resilience e second_phase_if_worse omitidas por brevidade, mas devem seguir a lógica Python)
std::pair<NodeID, NodeID> apply_resilience(Graph& G, const std::string& topo_name, NodeID leader, NodeID deputy, const std::string& action, const std::set<NodeID>& anomalies, const Config& cfg) {
    // ...
    return {leader, deputy};
}

void second_phase_if_worse(Graph& G, const std::string& topo_name, double delivery_before, double delivery_after, const std::set<NodeID>& anomalies, const Config& cfg) {
    // ...
}

// -------------------------------------
// 7. Loop Principal (Single Cycle e Run Multicycles)
// -------------------------------------

std::map<std::string, double> single_cycle(Graph& G, const std::string& topo_name, const Config& cfg, QValues& Q) {
    // 1-3. Formação, Eleição, Autenticação
    formation_phase(G);
    auto [leader, deputy] = elect_leaders(G);
    authenticate_leaders(G, leader, deputy);
    
    // 4. Operação pré-falha e Snapshot
    operation_step(G);
    std::map<NodeID, std::tuple<double, double, double>> pre_snapshot;
    for (const auto& pair : G.node_data) {
        const NodeAttrs& n = pair.second;
        pre_snapshot[pair.first] = {n.traffic, n.packet_rate, n.latency};
    }
    
    // 5. Falhas/Ataques
    std::set<NodeID> ground_truth = inject_failures_and_attacks(G, leader, ATTACK_CFG);
    
    // 6. Qualidade antes
    auto [qi_before, parts_before] = quality_index(G);
    
    // 7. Ação sugerida
    std::string action = action_from_quality(qi_before);
    
    // 8. Detecção
    auto [anomalies, scores] = detect_anomalies(G, pre_snapshot, ground_truth, cfg);
    
    // Métricas de detecção (Adaptação para C++ requer uma função para precision_recall_fscore_support)
    double prec = 0.0, rec = 0.0, f1 = 0.0;
    // ... (Cálculo de métricas omitido)

    // 9. Correção
    std::tie(leader, deputy) = apply_resilience(G, topo_name, leader, deputy, action, anomalies, cfg);
    
    // 10. Reconfig e 2ª Fase
    operation_step(G);
    auto [qi_mid, parts_mid] = quality_index(G);
    second_phase_if_worse(G, topo_name, parts_before.at("delivery_ratio"), parts_mid.at("delivery_ratio"), anomalies, cfg);
    operation_step(G); // 3ª operação (após 2ª fase)
    
    // 11. Retorno operacional (pós)
    auto [qi_after, parts_after] = quality_index(G);
    
    // Atualiza Q
    update_Q(Q, action, qi_before, qi_after, parts_before.at("delivery_ratio"), parts_after.at("delivery_ratio"));

    return {
        {"qi", qi_after},
        {"delivery", parts_after.at("delivery_ratio")},
        {"precision", prec},
        {"recall", rec},
        {"f1", f1},
        {"anomalies", (double)anomalies.size()},
        {"attacks", (double)ground_truth.size()}
        // Q-values seriam logados separadamente
    };
}

// -------------------------------------
// 8. Main (Estrutura de Execução)
// -------------------------------------

void run_multicycles(const Config& config, const std::string& label, int cycles) {
    // Definições de Configurações
    const std::map<std::string, double> BASELINE_CFG = {
        {"use_lof_ensemble", 0.0}, {"base_contamination", 0.10}, {"second_phase", 0.0},
        {"quality_aware_mesh", 0.0}, {"mesh_add_ER", 2.0}, {"mesh_add_WS", 2.0},
        {"mesh_add_BA", 2.0}, {"promote_deputy", 1.0}
    };
    const std::map<std::string, double> IMPROVED_CFG = {
        {"use_lof_ensemble", 1.0}, {"base_contamination", 0.10}, {"second_phase", 1.0},
        {"quality_aware_mesh", 1.0}, {"mesh_add_ER", 4.0}, {"mesh_add_WS", 6.0},
        {"mesh_add_BA", 4.0}, {"promote_deputy", 1.0}
    };
    
    const Config& actual_config = (label == "Baseline") ? BASELINE_CFG : IMPROVED_CFG;

    std::map<std::string, Graph> graphs = {
        {"Erdos-Renyi (ER)", build_topology("ER", N_NODES, RANDOM_SEED)},
        {"Watts-Strogatz (WS)", build_topology("WS", N_NODES, RANDOM_SEED + 1)},
        {"Barabasi-Albert (BA)", build_topology("BA", N_NODES, RANDOM_SEED + 2)},
    };

    std::map<std::string, std::map<std::string, std::vector<double>>> series;
    for (const auto& pair : graphs) {
        series[pair.first] = {
            {"qi", {}}, {"delivery", {}}, {"precision", {}},
            {"recall", {}}, {"f1", {}}, {"actions", {}}
        };
    }
    QValues Q = fresh_Q();

    for (auto& pair : graphs) {
        std::string name = pair.first;
        Graph& G = pair.second;
        for (int t = 0; t < cycles; ++t) {
            auto out = single_cycle(G, name, actual_config, Q);
            series[name]["qi"].push_back(out["qi"]);
            series[name]["delivery"].push_back(out["delivery"]);
            series[name]["precision"].push_back(out["precision"]);
            series[name]["recall"].push_back(out["recall"]);
            series[name]["f1"].push_back(out["f1"]);
            // Ação e Q-values seriam processados aqui
        }
    }
    // Lógica para print_summary e plot_time_series seria executada aqui.
    // Omitido, pois plotagem C++ exige biblioteca externa.
    std::cout << "\n===== Execução (" << label << ") Concluída =====" << std::endl;
    // ... (Impressão de resumos, semelhante ao Python)
}

int main() {
    // Configurações e execução principal (equivalente ao if __name__ == "__main__":)
    run_multicycles({}, "Baseline", N_CYCLES);
    run_multicycles({}, "Melhorado", N_CYCLES);

    // plot_compare_configs chamadas aqui...

    return 0;
}