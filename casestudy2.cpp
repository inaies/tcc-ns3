// resilience_sim.cpp
// Conversão/adaptação do script Python fornecido para C++17 (sem dependências externas).
// Compilar: g++ -std=c++17 -O2 -o resilience_sim resilience_sim.cpp

#include <bits/stdc++.h>
using namespace std;

// ---------------------------
// Configurações e utilidades
// ---------------------------
static const unsigned RANDOM_SEED = 42;
static std::mt19937 rng(RANDOM_SEED);

int N_NODES = 50;

struct NodeProps {
    double traffic;
    double packet_rate;
    double latency;
    double energy;
    string role; // "member", "leader", "deputy"
    bool authenticated;
    bool alive;
    int hello_count;
    NodeProps() {
        traffic = 0.0;
        packet_rate = 0.0;
        latency = 0.0;
        energy = 1.0;
        role = "member";
        authenticated = false;
        alive = true;
        hello_count = 0;
    }
};

struct EdgeProps {
    double quality;
    double capacity;
    EdgeProps() { quality = 1.0; capacity = 1.0; }
};

// Representação de grafo simples com adjacency sets
struct Graph {
    int n;
    vector<unordered_set<int>> adj;
    vector<NodeProps> nodes;
    // edge key: min(u,v) << 32 | max(u,v)
    unordered_map<uint64_t, EdgeProps> edges;

    Graph(int n_=0) { init(n_); }
    void init(int n_) {
        n = n_;
        adj.assign(n, {});
        nodes.assign(n, NodeProps());
        edges.clear();
    }
    static uint64_t edge_key(int u, int v) {
        if (u > v) std::swap(u, v);
        return ( (uint64_t)u << 32 ) | (uint32_t)v;
    }
    void add_edge(int u, int v, EdgeProps ep = EdgeProps()) {
        if (u==v) return;
        if (adj[u].count(v)) return;
        adj[u].insert(v);
        adj[v].insert(u);
        edges[edge_key(u,v)] = ep;
    }
    void remove_edge(int u, int v) {
        if (!adj[u].count(v)) return;
        adj[u].erase(v);
        adj[v].erase(u);
        edges.erase(edge_key(u,v));
    }
    bool has_edge(int u,int v) const {
        return adj[u].count(v);
    }
    vector<pair<int,int>> edge_list() const {
        vector<pair<int,int>> out;
        out.reserve(edges.size());
        for (auto &kv : edges) {
            uint64_t k = kv.first;
            int u = (int)(k >> 32);
            int v = (int)(k & 0xffffffffu);
            out.emplace_back(u,v);
        }
        return out;
    }
    int number_of_edges() const { return (int)edges.size(); }
    int number_of_nodes() const { return n; }
};

// Random helpers
double urand(double a=0.0, double b=1.0) {
    uniform_real_distribution<double> dist(a,b);
    return dist(rng);
}
int irand(int a, int b) {
    uniform_int_distribution<int> dist(a,b);
    return dist(rng);
}

// ---------------------------
// Topologias
// ---------------------------
Graph build_ER(int n, double p=0.10) {
    Graph G(n);
    for (int u=0; u<n; ++u) {
        for (int v=u+1; v<n; ++v) {
            if (urand() < p) {
                EdgeProps ep;
                ep.quality = urand(0.7, 1.0);
                ep.capacity = urand(1.0, 10.0);
                G.add_edge(u,v,ep);
            }
        }
    }
    return G;
}

// Watts-Strogatz: cada nó conectado aos k/2 vizinhos de cada lado, rewire prob beta
Graph build_WS(int n, int k=6, double beta=0.2) {
    Graph G(n);
    if (k % 2 == 1) k++; // assegurar par
    int half = k/2;
    for (int u=0; u<n; ++u) {
        for (int j=1; j<=half; ++j) {
            int v = (u + j) % n;
            EdgeProps ep; ep.quality = urand(0.7,1.0); ep.capacity = urand(1.0,10.0);
            G.add_edge(u, v, ep);
        }
    }
    // rewire
    for (int u=0; u<n; ++u) {
        for (int j=1; j<=half; ++j) {
            int v = (u + j) % n;
            if (u < v) {
                if (urand() < beta) {
                    // escolher um novo v' não igual a u e sem aresta ainda
                    int trials = 0;
                    while (trials < 10) {
                        int vp = irand(0, n-1);
                        if (vp == u) { ++trials; continue; }
                        if (!G.has_edge(u, vp)) {
                            EdgeProps ep; ep.quality = urand(0.7,1.0); ep.capacity = urand(1.0,10.0);
                            G.remove_edge(u,v);
                            G.add_edge(u,vp,ep);
                            break;
                        }
                        ++trials;
                    }
                }
            }
        }
    }
    return G;
}

// Barabasi-Albert (preferential attachment) com m novas arestas por nó
Graph build_BA(int n, int m=2) {
    Graph G(0);
    if (n <= m) {
        return build_ER(n, 1.0); // completo
    }
    int initial = m+1;
    G.init(initial);
    // start com clique inicial
    for (int u=0; u<initial; ++u) {
        for (int v=u+1; v<initial; ++v) {
            EdgeProps ep; ep.quality = urand(0.7,1.0); ep.capacity = urand(1.0,10.0);
            G.add_edge(u,v,ep);
        }
    }
    // vetor com repetição por grau (método simples)
    vector<int> degree_list;
    for (int u=0; u<initial; ++u) {
        for (int k=0; k<(int)G.adj[u].size(); ++k) degree_list.push_back(u);
    }
    for (int newv = initial; newv < n; ++newv) {
        G.adj.emplace_back();
        G.nodes.emplace_back();
        ++G.n;
        unordered_set<int> targets;
        // seletivamente amostra m nós (sem repetição)
        while ((int)targets.size() < m && !degree_list.empty()) {
            int idx = irand(0, (int)degree_list.size()-1);
            int cand = degree_list[idx];
            targets.insert(cand);
        }
        // se degree_list vazio (caso raro), escolhe aleatório
        while ((int)targets.size() < m) {
            int cand = irand(0, newv-1);
            if (cand != newv) targets.insert(cand);
        }
        for (int t : targets) {
            EdgeProps ep; ep.quality = urand(0.7,1.0); ep.capacity = urand(1.0,10.0);
            G.add_edge(newv, t, ep);
        }
        // atualiza degree_list
        for (int t : targets) {
            degree_list.push_back(t);
        }
        // novo nó aparece m vezes
        for (int k=0; k<m; ++k) degree_list.push_back(newv);
    }
    return G;
}

// Garante conectividade mínima: conecta componentes por arestas aleatórias
void ensure_connected(Graph &G) {
    int n = G.number_of_nodes();
    vector<int> comp_id(n, -1);
    int cid = 0;
    for (int i=0;i<n;++i) {
        if (comp_id[i] != -1) continue;
        // BFS
        queue<int>q; q.push(i); comp_id[i] = cid;
        while(!q.empty()) {
            int u=q.front(); q.pop();
            for (int v: G.adj[u]) {
                if (comp_id[v]==-1) { comp_id[v]=cid; q.push(v); }
            }
        }
        cid++;
    }
    if (cid <= 1) return;
    // gather nodes per component
    vector<vector<int>> comps(cid);
    for (int i=0;i<n;++i) comps[comp_id[i]].push_back(i);
    for (int i=0;i<cid-1;++i) {
        int a = comps[i][irand(0, (int)comps[i].size()-1)];
        int b = comps[i+1][irand(0, (int)comps[i+1].size()-1)];
        EdgeProps ep; ep.quality = urand(0.7, 1.0); ep.capacity = urand(1.0, 10.0);
        G.add_edge(a,b,ep);
    }
}

// Inicializa atributos de nós/arestas
void init_node_edge_attributes(Graph &G) {
    for (int v=0; v<G.number_of_nodes(); ++v) {
        G.nodes[v].traffic = urand(0.1, 1.0);
        G.nodes[v].packet_rate = urand(1.0, 50.0);
        G.nodes[v].latency = urand(5.0, 50.0);
        G.nodes[v].energy = urand(0.4, 1.0);
        G.nodes[v].role = "member";
        G.nodes[v].authenticated = false;
        G.nodes[v].alive = true;
    }
    for (auto &kv : G.edges) {
        kv.second.quality = urand(0.7, 1.0);
        kv.second.capacity = urand(1.0, 10.0);
    }
}

// ---------------------------
// Formação e mensagens
// ---------------------------
void formation_phase(Graph &G) {
    for (int v=0; v<G.number_of_nodes(); ++v) {
        G.nodes[v].hello_count = irand(3, 9);
    }
}

// ---------------------------
// Eleição de líder e vice
// ---------------------------
pair<int,int> elect_leaders(Graph &G) {
    int n = G.number_of_nodes();
    vector<pair<int,int>> deg; // (degree, node)
    for (int v=0; v<n; ++v) deg.emplace_back((int)G.adj[v].size(), v);
    sort(deg.begin(), deg.end(), [](auto &a, auto &b){ if(a.first!=b.first) return a.first>b.first; return a.second<b.second; });
    int leader = deg.size() ? deg[0].second : -1;
    int deputy = deg.size() > 1 ? deg[1].second : -1;
    for (int v=0; v<n; ++v) G.nodes[v].role = "member";
    if (leader>=0) G.nodes[leader].role = "leader";
    if (deputy>=0) G.nodes[deputy].role = "deputy";
    return {leader, deputy};
}

// ---------------------------
// Autenticação
// ---------------------------
bool authenticate_leaders(Graph &G, int leader, int deputy) {
    auto check = [&](int node)->bool{
        if (node < 0 || node >= G.number_of_nodes()) return false;
        return (G.nodes[node].energy > 0.5) && (G.nodes[node].latency < 40.0);
    };
    if (leader>=0) G.nodes[leader].authenticated = check(leader);
    if (deputy>=0) G.nodes[deputy].authenticated = check(deputy);
    return (leader>=0 && deputy>=0 && G.nodes[leader].authenticated && G.nodes[deputy].authenticated);
}

// ---------------------------
// Operação
// ---------------------------
void operation_step(Graph &G) {
    for (int v=0; v<G.number_of_nodes(); ++v) {
        if (!G.nodes[v].alive) continue;
        normal_distribution<double> nd(1.0, 0.1);
        double load_factor = nd(rng);
        if (std::isnan(load_factor)) load_factor = 1.0;
        load_factor = max(0.5, min(1.5, load_factor));
        G.nodes[v].traffic = min(5.0, max(0.05, G.nodes[v].traffic * load_factor));
        G.nodes[v].packet_rate = min(200.0, max(0.5, G.nodes[v].packet_rate * load_factor));
        G.nodes[v].latency = min(200.0, max(2.0, G.nodes[v].latency * (2.0 - load_factor)));
        G.nodes[v].energy = max(0.0, G.nodes[v].energy - 0.005 * load_factor);
    }
    // degrade quality of edges
    vector<uint64_t> keys;
    keys.reserve(G.edges.size());
    for (auto &kv : G.edges) keys.push_back(kv.first);
    for (auto k : keys) {
        double dec = urand(0.0, 0.01);
        G.edges[k].quality = max(0.2, min(1.0, G.edges[k].quality - dec));
    }
}

// ---------------------------
// Injeção de falhas/ataques
// ---------------------------
struct AttackConfig {
    int num_node_failures = 4;
    int num_link_failures = 6;
    int num_dos_nodes = 5;
    bool attack_leader = true;
};

set<int> inject_failures_and_attacks(Graph &G, int leader, const AttackConfig &cfg) {
    set<int> ground_truth;
    int n = G.number_of_nodes();
    // choose failed nodes
    vector<int> candidates;
    for (int i=0;i<n;++i) candidates.push_back(i);
    shuffle(candidates.begin(), candidates.end(), rng);
    int kn = min((int)candidates.size(), cfg.num_node_failures);
    for (int i=0;i<kn;++i) {
        int v = candidates[i];
        G.nodes[v].alive = false;
        G.nodes[v].traffic = 0.0;
        // remove incident edges
        vector<int> neigh(G.adj[v].begin(), G.adj[v].end());
        for (int nbr: neigh) {
            if (G.has_edge(v,nbr)) G.remove_edge(v,nbr);
        }
    }
    // link failures
    auto edges = G.edge_list();
    shuffle(edges.begin(), edges.end(), rng);
    for (int i=0; i<cfg.num_link_failures && i < (int)edges.size(); ++i) {
        G.remove_edge(edges[i].first, edges[i].second);
    }
    // DoS nodes
    vector<int> alive_nodes;
    for (int i=0;i<n;++i) if (G.nodes[i].alive) alive_nodes.push_back(i);
    shuffle(alive_nodes.begin(), alive_nodes.end(), rng);
    int kd = min((int)alive_nodes.size(), cfg.num_dos_nodes);
    for (int i=0;i<kd;++i) {
        int v = alive_nodes[i];
        G.nodes[v].traffic *= urand(5.0, 15.0);
        G.nodes[v].latency *= urand(1.2, 2.0);
        ground_truth.insert(v);
    }
    // attack leader
    if (cfg.attack_leader && leader >= 0 && leader < n && G.nodes[leader].alive) {
        G.nodes[leader].traffic *= urand(5.0, 10.0);
        G.nodes[leader].latency *= urand(2.0, 4.0);
        ground_truth.insert(leader);
    }
    return ground_truth;
}

// ---------------------------
// Avaliação de qualidade
// ---------------------------
double largest_component_fraction(const Graph &G) {
    int n = G.number_of_nodes();
    if (n == 0) return 0.0;
    vector<int> comp(n, -1);
    int cid = 0;
    for (int i=0;i<n;++i) {
        if (comp[i] != -1) continue;
        // BFS
        queue<int>q; q.push(i); comp[i]=cid;
        while(!q.empty()){
            int u=q.front(); q.pop();
            for (int v: G.adj[u]) {
                if (comp[v]==-1) { comp[v]=cid; q.push(v); }
            }
        }
        cid++;
    }
    vector<int> sizes(cid,0);
    for (int i=0;i<n;++i) if (comp[i]>=0) sizes[comp[i]]++;
    int maxs = 0;
    for (int s: sizes) maxs = max(maxs, s);
    return (double)maxs / (double)n;
}

// retorna nós do maior componente
vector<int> largest_component_nodes(const Graph &G) {
    int n = G.number_of_nodes();
    if (n==0) return {};
    vector<int> comp(n, -1);
    int cid = 0;
    for (int i=0;i<n;++i) {
        if (comp[i]!=-1) continue;
        queue<int>q; q.push(i); comp[i]=cid;
        while(!q.empty()) {
            int u=q.front(); q.pop();
            for (int v:G.adj[u]) if (comp[v]==-1) { comp[v]=cid; q.push(v); }
        }
        cid++;
    }
    vector<int> sizes(cid,0);
    for (int i=0;i<n;++i) if (comp[i]>=0) sizes[comp[i]]++;
    int maxi = 0;
    for (int i=1;i<cid;++i) if (sizes[i] > sizes[maxi]) maxi = i;
    vector<int> out;
    for (int i=0;i<n;++i) if (comp[i] == maxi) out.push_back(i);
    return out;
}

// APSP via BFS (since grafo é não ponderado) para o componente gigante
double propagation_efficiency(const Graph &G) {
    auto giant = largest_component_nodes(G);
    if (giant.size() <= 1) return 0.0;
    unordered_set<int> Hset(giant.begin(), giant.end());
    // map node -> BFS distances to others
    vector<int> nodes = giant;
    double sum_inv = 0.0;
    int count = 0;
    for (int u : nodes) {
        // BFS from u
        vector<int> dist(G.number_of_nodes(), -1);
        queue<int>q;
        dist[u] = 0; q.push(u);
        while(!q.empty()) {
            int x = q.front(); q.pop();
            for (int v: G.adj[x]) {
                if (!Hset.count(v)) continue;
                if (dist[v] == -1) {
                    dist[v] = dist[x] + 1;
                    q.push(v);
                }
            }
        }
        for (int v: nodes) {
            if (v==u) continue;
            int d = dist[v];
            if (d > 0) { sum_inv += 1.0 / (double)d; count++; }
        }
    }
    if (count == 0) return 0.0;
    return sum_inv / (double)count;
}

double delivery_ratio_proxy(const Graph &G) {
    if (G.number_of_edges() == 0) return 0.0;
    auto giant = largest_component_nodes(G);
    unordered_set<int> gs(giant.begin(), giant.end());
    vector<double> vals;
    for (auto &kv : G.edges) {
        uint64_t key = kv.first;
        int u = (int)(key >> 32);
        int v = (int)(key & 0xffffffffu);
        if (gs.count(u) && gs.count(v)) {
            vals.push_back(kv.second.quality * kv.second.capacity);
        }
    }
    if (vals.empty()) return 0.0;
    double mean = 0.0;
    for (double x: vals) mean += x;
    mean /= (double)vals.size();
    return mean / 10.0; // normaliza como no python
}

struct QualityParts {
    double connectivity_frac;
    double prop_eff;
    double delivery_ratio;
};

pair<double, QualityParts> quality_index(const Graph &G) {
    double c = largest_component_fraction(G);
    double p = propagation_efficiency(G);
    double d = delivery_ratio_proxy(G);
    double qi = 0.5*c + 0.3*p + 0.2*d;
    QualityParts parts{c,p,d};
    return {qi, parts};
}

// ---------------------------
// Ações a partir do QI
// ---------------------------
string action_from_quality(double qi) {
    if (qi >= 0.75) return "OK";
    else if (qi >= 0.55) return "REINFORCE_MESH";
    else if (qi >= 0.35) return "ISOLATE_SUSPECTS_AND_REWIRE";
    else return "PROMOTE_DEPUTY_AND_REWIRE";
}

// ---------------------------
// Detecção de anomalias (substitui IsolationForest)
// Implementação simples: para cada feature, calcula média e std em nós vivos,
// e marca como anomalia nós que têm |z| > threshold em qualquer feature.
// ---------------------------
struct DetectResult {
    set<int> anomalies;
    unordered_map<int,double> score_map;
};

DetectResult detect_anomalies(const Graph &G, double contamination = 0.1) {
    int n = G.number_of_nodes();
    vector<int> alive;
    for (int v=0; v<n; ++v) if (G.nodes[v].alive) alive.push_back(v);
    DetectResult res;
    if (alive.empty()) return res;

    int m = (int)alive.size();
    // features: traffic, packet_rate, latency, energy
    vector<double> mean(4,0.0), var(4,0.0);
    vector<vector<double>> X(m, vector<double>(4));
    for (int i=0;i<m;++i) {
        NodeProps const &p = G.nodes[alive[i]];
        X[i][0] = p.traffic;
        X[i][1] = p.packet_rate;
        X[i][2] = p.latency;
        X[i][3] = p.energy;
        for (int f=0;f<4;++f) mean[f] += X[i][f];
    }
    for (int f=0;f<4;++f) mean[f] /= (double)m;
    for (int i=0;i<m;++i) for (int f=0;f<4;++f) var[f] += (X[i][f]-mean[f])*(X[i][f]-mean[f]);
    for (int f=0;f<4;++f) var[f] = (var[f] / (double)m);
    vector<double> stdv(4, 1e-9);
    for (int f=0;f<4;++f) stdv[f] = sqrt(max(1e-9, var[f]));

    double threshold = 2.5; // z-score threshold
    vector<double> anomaly_scores(m, 0.0);
    for (int i=0;i<m;++i) {
        double maxz = 0.0;
        for (int f=0;f<4;++f) {
            double z = fabs((X[i][f] - mean[f]) / stdv[f]);
            maxz = max(maxz, z);
        }
        anomaly_scores[i] = maxz;
    }
    // decide top-k as anomalies by contamination (approx)
    vector<int> idxs(m);
    iota(idxs.begin(), idxs.end(), 0);
    sort(idxs.begin(), idxs.end(), [&](int a, int b){ return anomaly_scores[a] > anomaly_scores[b]; });
    int n_anom = max(1, (int)round(contamination * m));
    for (int k=0;k<n_anom && k<m;++k) {
        int i = idxs[k];
        res.anomalies.insert(alive[i]);
    }
    for (int i=0;i<m;++i) {
        res.score_map[alive[i]] = anomaly_scores[i];
    }
    return res;
}

// ---------------------------
// Aplicar resiliência
// ---------------------------
struct ResilienceConfig {
    int add_mesh_edges = 6;
    bool promote_deputy = true;
    bool isolate_malicious = true;
};

pair<int,int> apply_resilience(Graph &G, int leader, int deputy, const string &action,
                               const set<int> &anomalies, const unordered_map<int,double> &score_map,
                               const ResilienceConfig &cfg) {
    // isola suspeitos (remove arestas)
    if (cfg.isolate_malicious && (action.find("ISOLATE") != string::npos || action.find("REWIRE") != string::npos)) {
        for (int v : anomalies) {
            vector<int> neigh(G.adj[v].begin(), G.adj[v].end());
            for (int nbr : neigh) {
                if (G.has_edge(v,nbr)) G.remove_edge(v,nbr);
            }
        }
    }
    // promove deputy se necessário
    if (cfg.promote_deputy && (action.find("PROMOTE") != string::npos || leader < 0 || leader >= G.number_of_nodes() || !G.nodes[leader].alive)) {
        if (deputy >=0 && deputy < G.number_of_nodes() && G.nodes[deputy].alive) {
            for (int v=0; v<G.number_of_nodes(); ++v) if (G.nodes[v].role == "leader") G.nodes[v].role = "member";
            G.nodes[deputy].role = "leader";
            leader = deputy;
            // escolhe novo deputy pelo maior grau restante
            vector<pair<int,int>> deg;
            for (int v=0; v<G.number_of_nodes(); ++v) deg.emplace_back((int)G.adj[v].size(), v);
            sort(deg.begin(), deg.end(), [](auto &a, auto &b){ if (a.first != b.first) return a.first > b.first; return a.second < b.second; });
            for (auto &pr : deg) {
                if (pr.second != leader) { deputy = pr.second; G.nodes[deputy].role = "deputy"; break; }
            }
        }
    }
    // reforça malha
    if (cfg.add_mesh_edges > 0 && (action.find("REINFORCE") != string::npos || action.find("REWIRE") != string::npos)) {
        if (G.number_of_nodes() > 1) {
            auto comp = largest_component_nodes(G);
            unordered_set<int> Hset(comp.begin(), comp.end());
            // sample non-edges among comp
            vector<pair<int,int>> nonedges;
            for (int i=0;i<(int)comp.size(); ++i) {
                for (int j=i+1;j<(int)comp.size(); ++j) {
                    int u = comp[i], v = comp[j];
                    if (!G.has_edge(u,v)) nonedges.emplace_back(u,v);
                }
            }
            shuffle(nonedges.begin(), nonedges.end(), rng);
            int added = 0;
            for (auto &pr : nonedges) {
                if (added >= cfg.add_mesh_edges) break;
                EdgeProps ep; ep.quality = urand(0.6, 0.9); ep.capacity = urand(2.0, 8.0);
                G.add_edge(pr.first, pr.second, ep);
                added++;
            }
        }
    }
    return {leader, deputy};
}

// ---------------------------
// Métricas de detecção (precision/recall/f1)
// ---------------------------
struct PRF {
    double precision;
    double recall;
    double f1;
};
PRF compute_prf(const set<int>& ground_truth, const set<int>& detected, const set<int>& universe) {
    int tp = 0, fp = 0, fn = 0;
    for (int v : universe) {
        bool g = ground_truth.count(v);
        bool d = detected.count(v);
        if (g && d) tp++;
        if (!g && d) fp++;
        if (g && !d) fn++;
    }
    double prec = (tp + fp) ? (double)tp / (double)(tp + fp) : 0.0;
    double rec = (tp + fn) ? (double)tp / (double)(tp + fn) : 0.0;
    double f1 = (prec + rec) ? 2.0 * prec * rec / (prec + rec) : 0.0;
    return {prec, rec, f1};
}

// ---------------------------
// single_cycle (um ciclo completo)
// ---------------------------
struct CycleResult {
    string topology;
    bool auth_ok;
    int leader;
    int deputy;
    double qi_before;
    double qi_after;
    double recovery_gain;
    double connectivity_before;
    double connectivity_after;
    double prop_eff_before;
    double prop_eff_after;
    double delivery_before;
    double delivery_after;
    string action;
    int ground_truth_attacks;
    int detected_anomalies;
    double precision;
    double recall;
    double f1;
    double lcc_before;
    double lcc_after;
};

CycleResult single_cycle(Graph G, const string &topo_name,
                         const AttackConfig &attack_cfg = AttackConfig(),
                         const ResilienceConfig &res_cfg = ResilienceConfig()) {
    CycleResult res;
    res.topology = topo_name;
    // 1 Formação
    formation_phase(G);
    // 2 Eleição
    auto pr = elect_leaders(G);
    int leader = pr.first, deputy = pr.second;
    // 3 Autenticação
    bool auth_ok = authenticate_leaders(G, leader, deputy);
    // 4 Operação (pré-falha)
    operation_step(G);
    // 5 Falhas/Ataques
    auto ground_truth = inject_failures_and_attacks(G, leader, attack_cfg);
    // 6 QI antes
    auto qbefore = quality_index(G);
    double qi_before = qbefore.first;
    auto parts_before = qbefore.second;
    // 7 Ação
    string action = action_from_quality(qi_before);
    // 8 Detecção
    auto det = detect_anomalies(G, 0.1);
    auto anomalies = det.anomalies;
    auto scores = det.score_map;
    // métricas de detecção
    set<int> all_nodes_alive_or_attacked;
    for (int v=0; v<G.number_of_nodes(); ++v) if (G.nodes[v].alive) all_nodes_alive_or_attacked.insert(v);
    for (int v : ground_truth) all_nodes_alive_or_attacked.insert(v);
    auto prf = compute_prf(ground_truth, anomalies, all_nodes_alive_or_attacked);
    // 9 Correção
    auto leader_deputy_after = apply_resilience(G, leader, deputy, action, anomalies, scores, res_cfg);
    leader = leader_deputy_after.first; deputy = leader_deputy_after.second;
    // 10 Reconfiguração (mais um passo)
    operation_step(G);
    // 11 QI depois
    auto qafter = quality_index(G);
    double qi_after = qafter.first;
    auto parts_after = qafter.second;
    double recovery_gain = qi_after - qi_before;
    // montar res
    res.auth_ok = auth_ok;
    res.leader = leader;
    res.deputy = deputy;
    res.qi_before = qi_before;
    res.qi_after = qi_after;
    res.recovery_gain = recovery_gain;
    res.connectivity_before = parts_before.connectivity_frac;
    res.connectivity_after = parts_after.connectivity_frac;
    res.prop_eff_before = parts_before.prop_eff;
    res.prop_eff_after = parts_after.prop_eff;
    res.delivery_before = parts_before.delivery_ratio;
    res.delivery_after = parts_after.delivery_ratio;
    res.action = action;
    res.ground_truth_attacks = (int)ground_truth.size();
    res.detected_anomalies = (int)anomalies.size();
    res.precision = prf.precision;
    res.recall = prf.recall;
    res.f1 = prf.f1;
    res.lcc_before = parts_before.connectivity_frac;
    res.lcc_after = parts_after.connectivity_frac;
    return res;
}

// ---------------------------
// run_all
// ---------------------------
map<string, CycleResult> run_all_topologies() {
    map<string, CycleResult> results;
    // 1 ER
    Graph er = build_ER(N_NODES, 0.10);
    ensure_connected(er);
    init_node_edge_attributes(er);
    auto r_er = single_cycle(er, "Erdos-Renyi (ER)");
    results["Erdos-Renyi (ER)"] = r_er;

    // 2 WS
    Graph ws = build_WS(N_NODES, 6, 0.2);
    ensure_connected(ws);
    init_node_edge_attributes(ws);
    auto r_ws = single_cycle(ws, "Watts-Strogatz (WS)");
    results["Watts-Strogatz (WS)"] = r_ws;

    // 3 BA
    Graph ba = build_BA(N_NODES, 2);
    ensure_connected(ba);
    init_node_edge_attributes(ba);
    auto r_ba = single_cycle(ba, "Barabasi-Albert (BA)");
    results["Barabasi-Albert (BA)"] = r_ba;

    return results;
}

// ---------------------------
// Impressão e visualização (básica)
// ---------------------------
void print_summary(const map<string, CycleResult> &results) {
    for (auto &kv : results) {
        auto &name = kv.first;
        auto &r = kv.second;
        cout << "\n=== " << name << " ===\n";
        cout << "Ação tomada: " << r.action << "\n";
        cout.setf(std::ios::fixed); cout<<setprecision(3);
        cout << "QI antes/depois: " << r.qi_before << " -> " << r.qi_after << " (ganho " << r.recovery_gain << ")\n";
        cout << "Conectividade LCC antes/depois: " << setprecision(2) << r.lcc_before << " -> " << r.lcc_after << "\n";
        cout << setprecision(3);
        cout << "Eficiência Propagação antes/depois: " << r.prop_eff_before << " -> " << r.prop_eff_after << "\n";
        cout << "Razão de Entrega antes/depois: " << r.delivery_before << " -> " << r.delivery_after << "\n";
        cout << setprecision(2);
        cout << "Detecção - Precision: " << r.precision << " | Recall: " << r.recall << " | F1: " << r.f1 << "\n";
        cout << "Ground-truth ataques: " << r.ground_truth_attacks << " | Anomalias detectadas: " << r.detected_anomalies << "\n";
        cout << "Autenticação líder/deputy OK? " << (r.auth_ok ? "YES" : "NO") << "\n";
    }
}

// ---------------------------
// main
// ---------------------------
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cout << "Simulação de resiliência (C++) - adaptado do script Python\n";
    N_NODES = 50; // ajustar aqui se necessário

    auto results = run_all_topologies();
    print_summary(results);

    cout << "\nFim da simulação.\n";
    return 0;
}
