# -*- coding: utf-8 -*-
import random
import time
from collections import deque

import numpy as np
import networkx as nx
import matplotlib.pyplot as plt
from sklearn.ensemble import IsolationForest
from sklearn.metrics import precision_recall_fscore_support

# ---------------------------
# Configurações e Utilidades
# ---------------------------
RANDOM_SEED = 42
random.seed(RANDOM_SEED)
np.random.seed(RANDOM_SEED)

N_NODES = 50
ATTACK_CONFIG = {
    "num_node_failures": 4,      # nós que falham (queda de energia/hardware)
    "num_link_failures": 6,      # links que caem (interferência)
    "num_dos_nodes": 5,          # nós sob DoS (explosão de tráfego)
    "attack_leader": True,       # tenta atacar o líder
}
RESILIENCE_CONFIG = {
    "add_mesh_edges": 6,         # arestas extras para reforçar malha na recuperação
    "promote_deputy": True,      # promove vice-líder se o líder cair
    "isolate_malicious": True,   # isola nós detectados como maliciosos (desabilita suas arestas)
}

# ---------------------------
# 1) Geração de Topologias
# ---------------------------
def build_topology(kind="ER", n=N_NODES):
    if kind == "ER":
        # Erdős–Rényi: aleatória
        p = 0.10
        G = nx.gnp_random_graph(n, p, seed=RANDOM_SEED)
    elif kind == "WS":
        # Watts–Strogatz: pequeno mundo
        k = 6  # vizinhos
        beta = 0.2  # prob. de rewiring
        G = nx.watts_strogatz_graph(n, k, beta, seed=RANDOM_SEED)
    elif kind == "BA":
        # Barabási–Albert: scale-free com hubs
        m = 2  # novas arestas por nó
        G = nx.barabasi_albert_graph(n, m, seed=RANDOM_SEED)
    else:
        raise ValueError("Topologia desconhecida.")

    # garante conectividade mínima adicionando arestas se necessário
    if not nx.is_connected(G):
        components = list(nx.connected_components(G))
        for i in range(len(components) - 1):
            a = random.choice(list(components[i]))
            b = random.choice(list(components[i + 1]))
            G.add_edge(a, b)

    # atributos básicos
    for v in G.nodes:
        G.nodes[v].update({
            "traffic": np.random.uniform(0.1, 1.0),
            "packet_rate": np.random.uniform(1, 50),
            "latency": np.random.uniform(5, 50),
            "energy": np.random.uniform(0.4, 1.0),
            "role": "member",      # member | leader | deputy
            "authenticated": False,
            "alive": True,
        })
    for u, v in G.edges:
        G.edges[u, v]["quality"] = np.random.uniform(0.7, 1.0)  # qualidade de link (0-1)
        G.edges[u, v]["capacity"] = np.random.uniform(1.0, 10.0)

    return G

# ---------------------------
# 2) Formação e Mensagens
# ---------------------------
def formation_phase(G):
    # simula troca de mensagens de hello/neighbor discovery (apenas contadores)
    for v in G.nodes:
        G.nodes[v]["hello_count"] = np.random.randint(3, 10)
    return G

# ---------------------------
# 3) Eleição de Líder e Vice
# ---------------------------
def elect_leaders(G):
    # usa centralidade de grau para escolher líder; vice-líder é o segundo
    deg = dict(G.degree())
    ordered = sorted(deg.items(), key=lambda x: x[1], reverse=True)
    leader = ordered[0][0]
    deputy = ordered[1][0]

    for v in G.nodes:
        G.nodes[v]["role"] = "member"
    G.nodes[leader]["role"] = "leader"
    G.nodes[deputy]["role"] = "deputy"
    return leader, deputy

# ---------------------------
# 4) Autenticação no IDS-Gateway
# ---------------------------
def authenticate_leaders(G, leader, deputy):
    # autenticação simples: checagem de energia e latência
    for node in [leader, deputy]:
        ok = (G.nodes[node]["energy"] > 0.5) and (G.nodes[node]["latency"] < 40)
        G.nodes[node]["authenticated"] = bool(ok)
    return G.nodes[leader]["authenticated"] and G.nodes[deputy]["authenticated"]

# ---------------------------
# 5) Operação: Coleta e Disseminação
# ---------------------------
def operation_step(G):
    # atualiza tráfego, latência e energia simulando operação
    for v in G.nodes:
        if not G.nodes[v]["alive"]:
            continue
        load_factor = np.clip(np.random.normal(1.0, 0.1), 0.5, 1.5)
        G.nodes[v]["traffic"] = np.clip(G.nodes[v]["traffic"] * load_factor, 0.05, 5.0)
        G.nodes[v]["packet_rate"] = np.clip(G.nodes[v]["packet_rate"] * load_factor, 0.5, 200)
        G.nodes[v]["latency"] = np.clip(G.nodes[v]["latency"] * (2 - load_factor), 2, 200)
        G.nodes[v]["energy"] = max(0.0, G.nodes[v]["energy"] - 0.005 * load_factor)

    # degrada qualidade de link levemente por uso
    for u, v in G.edges:
        G.edges[u, v]["quality"] = np.clip(G.edges[u, v]["quality"] - np.random.uniform(0.0, 0.01), 0.2, 1.0)

# ---------------------------
# 6) Injeção de Falhas/Ataques
# ---------------------------
def inject_failures_and_attacks(G, leader, config=ATTACK_CONFIG):
    ground_truth_attack_nodes = set()

    # falhas de nós (desligados)
    failed_nodes = random.sample(list(G.nodes), k=config["num_node_failures"])
    for v in failed_nodes:
        G.nodes[v]["alive"] = False
        G.nodes[v]["traffic"] = 0.0
        # remove arestas incidentes (simula indisponibilidade)
        for nbr in list(G.neighbors(v)):
            if G.has_edge(v, nbr):
                G.remove_edge(v, nbr)

    # falhas de links
    for _ in range(config["num_link_failures"]):
        if G.number_of_edges() == 0:
            break
        e = random.choice(list(G.edges))
        G.remove_edge(*e)

    # DoS em nós
    dos_nodes = random.sample([v for v in G.nodes if G.nodes[v]["alive"]],
                              k=min(config["num_dos_nodes"], len([v for v in G.nodes if G.nodes[v]["alive"]])))
    for v in dos_nodes:
        G.nodes[v]["traffic"] *= np.random.uniform(5, 15)  # explosão de tráfego
        G.nodes[v]["latency"] *= np.random.uniform(1.2, 2.0)
        ground_truth_attack_nodes.add(v)

    # ataque ao líder
    if config.get("attack_leader", True):
        # se líder ainda existe/alive, sofre perturbação
        if G.nodes[leader]["alive"]:
            G.nodes[leader]["traffic"] *= np.random.uniform(5, 10)
            G.nodes[leader]["latency"] *= np.random.uniform(2, 4)
            ground_truth_attack_nodes.add(leader)

    return ground_truth_attack_nodes

# ---------------------------
# 7) Avaliação de Qualidade
# ---------------------------
def largest_component_fraction(G):
    comps = list(nx.connected_components(G))
    if not comps:
        return 0.0
    giant = max(comps, key=len)
    return len(giant) / G.number_of_nodes()

def propagation_efficiency(G):
    # média do inverso da menor distância entre pares (no componente gigante)
    if G.number_of_nodes() == 0:
        return 0.0
    giant_nodes = max(nx.connected_components(G), key=len)
    H = G.subgraph(giant_nodes).copy()
    if H.number_of_nodes() <= 1:
        return 0.0
    lengths = dict(nx.all_pairs_shortest_path_length(H))
    invs = []
    for u in H.nodes:
        for v in H.nodes:
            if u == v:
                continue
            d = lengths[u].get(v, None)
            if d and d > 0:
                invs.append(1.0 / d)
    return float(np.mean(invs)) if invs else 0.0

def delivery_ratio_proxy(G):
    # proxy de entrega: média (qualidade de link * capacidade) no gigante
    if G.number_of_edges() == 0:
        return 0.0
    giant_nodes = max(nx.connected_components(G), key=len)
    E = [e for e in G.edges if e[0] in giant_nodes and e[1] in giant_nodes]
    if not E:
        return 0.0
    vals = [G.edges[u, v]["quality"] * G.edges[u, v]["capacity"] for u, v in E]
    # normaliza pelo máximo possível aproximado
    return float(np.mean(vals) / 10.0)  # 10 ~ capacidade máx

def quality_index(G):
    c = largest_component_fraction(G)
    p = propagation_efficiency(G)
    d = delivery_ratio_proxy(G)
    # índice composto simples (ponderações podem ser ajustadas)
    qi = 0.5 * c + 0.3 * p + 0.2 * d
    return qi, {"connectivity_frac": c, "prop_eff": p, "delivery_ratio": d}

# ---------------------------
# 8) Tabela de Ações (Regra)
# ---------------------------
def action_from_quality(qi):
    # thresholds simples
    if qi >= 0.75:
        return "OK"
    elif qi >= 0.55:
        return "REINFORCE_MESH"
    elif qi >= 0.35:
        return "ISOLATE_SUSPECTS_AND_REWIRE"
    else:
        return "PROMOTE_DEPUTY_AND_REWIRE"

# ---------------------------
# 9) Detecção e Correções
# ---------------------------
def detect_anomalies(G):
    # usa IsolationForest em features por nó (apenas nós vivos)
    alive_nodes = [v for v in G.nodes if G.nodes[v]["alive"]]
    if not alive_nodes:
        return set(), {}, None

    X = []
    idx = []
    for v in alive_nodes:
        X.append([G.nodes[v]["traffic"], G.nodes[v]["packet_rate"], G.nodes[v]["latency"], G.nodes[v]["energy"]])
        idx.append(v)

    clf = IsolationForest(contamination=0.1, random_state=RANDOM_SEED)
    clf.fit(X)
    y_pred = clf.predict(X)  # 1 normal, -1 anomalia
    scores = clf.decision_function(X)

    anomalies = set([idx[i] for i, y in enumerate(y_pred) if y == -1])
    score_map = {idx[i]: scores[i] for i in range(len(idx))}
    return anomalies, score_map, clf

def apply_resilience(G, leader, deputy, action, anomalies, score_map, config=RESILIENCE_CONFIG):
    # isola suspeitos (remove arestas)
    if config["isolate_malicious"] and ("ISOLATE" in action or "REWIRE" in action):
        for v in anomalies:
            for nbr in list(G.neighbors(v)):
                if G.has_edge(v, nbr):
                    G.remove_edge(v, nbr)

    # promove vice-líder se necessário
    if config["promote_deputy"] and (("PROMOTE" in action) or (leader not in G.nodes) or (not G.nodes[leader]["alive"])):
        if deputy in G.nodes and G.nodes[deputy]["alive"]:
            for v in G.nodes:
                if G.nodes[v]["role"] == "leader":
                    G.nodes[v]["role"] = "member"
            G.nodes[deputy]["role"] = "leader"
            leader = deputy
            # escolhe novo deputy pelo maior grau restante
            deg = dict(G.degree())
            ordered = sorted(deg.items(), key=lambda x: x[1], reverse=True)
            for cand, _ in ordered:
                if cand != leader:
                    G.nodes[cand]["role"] = "deputy"
                    deputy = cand
                    break

    # reforça malha: adiciona arestas entre pares mais distantes (no gigante)
    if config["add_mesh_edges"] > 0 and ("REINFORCE" in action or "REWIRE" in action):
        if G.number_of_nodes() > 1:
            comp = max(nx.connected_components(G), key=len)
            H = G.subgraph(comp).copy()
            pairs = list(nx.non_edges(H))
            random.shuffle(pairs)
            # tenta ligar pares com maior distância aproximada (usa amostra aleatória)
            added = 0
            for (u, v) in pairs:
                if added >= config["add_mesh_edges"]:
                    break
                G.add_edge(u, v, quality=np.random.uniform(0.6, 0.9), capacity=np.random.uniform(2.0, 8.0))
                added += 1

    return leader, deputy

# ---------------------------
# 10) Loop de um Ciclo Completo
# ---------------------------
def single_cycle(G, topo_name):
    # 1. Formação
    formation_phase(G)

    # 2. Eleição
    leader, deputy = elect_leaders(G)

    # 3. Autenticação
    auth_ok = authenticate_leaders(G, leader, deputy)

    # 4. Operação (pré-falha)
    operation_step(G)

    # 5. Falhas/Ataques
    ground_truth = inject_failures_and_attacks(G, leader, ATTACK_CONFIG)

    # 6. Queda de Qualidade
    qi_before, parts_before = quality_index(G)

    # 7. Verificação e Ação
    action = action_from_quality(qi_before)

    # 8. Detecção de anomalias
    anomalies, scores, _ = detect_anomalies(G)

    # Métricas de detecção (comparando com ground_truth)
    y_true = []
    y_pred = []
    all_nodes_alive_or_attacked = set([v for v in G.nodes if G.nodes[v]["alive"]]) | ground_truth
    for v in all_nodes_alive_or_attacked:
        y_true.append(1 if v in ground_truth else 0)
        y_pred.append(1 if v in anomalies else 0)
    if len(set(y_true)) == 2:
        prec, rec, f1, _ = precision_recall_fscore_support(y_true, y_pred, average='binary', zero_division=0)
    else:
        prec = rec = f1 = 0.0

    # 9. Correção
    leader, deputy = apply_resilience(G, leader, deputy, action, anomalies, scores, RESILIENCE_CONFIG)

    # 10. Reconfiguração (simplesmente roda mais um passo de operação)
    operation_step(G)

    # 11. Retorno ao operacional (qualidade pós-correção)
    qi_after, parts_after = quality_index(G)

    # Tempo de recuperação (proxy): diferença entre índices
    recovery_gain = qi_after - qi_before

    results = {
        "topology": topo_name,
        "auth_ok": auth_ok,
        "leader": leader,
        "deputy": deputy,
        "qi_before": qi_before,
        "qi_after": qi_after,
        "recovery_gain": recovery_gain,
        "connectivity_before": parts_before["connectivity_frac"],
        "connectivity_after": parts_after["connectivity_frac"],
        "prop_eff_before": parts_before["prop_eff"],
        "prop_eff_after": parts_after["prop_eff"],
        "delivery_before": parts_before["delivery_ratio"],
        "delivery_after": parts_after["delivery_ratio"],
        "action": action,
        "ground_truth_attacks": len(ground_truth),
        "detected_anomalies": len(anomalies),
        "precision": prec,
        "recall": rec,
        "f1": f1,
        "lcc_before": parts_before["connectivity_frac"],
        "lcc_after": parts_after["connectivity_frac"],
    }
    return results, G

# ---------------------------
# 11) Execução nas 3 Topologias
# ---------------------------
def run_all():
    topologies = {
        "Erdos-Renyi (ER)": build_topology("ER", N_NODES),
        "Watts-Strogatz (WS)": build_topology("WS", N_NODES),
        "Barabasi-Albert (BA)": build_topology("BA", N_NODES),
    }

    results = {}
    graphs = {}

    for name, G in topologies.items():
        res, Gout = single_cycle(G, name)
        results[name] = res
        graphs[name] = Gout

    return results, graphs

# ---------------------------
# 12) Visualizações
# ---------------------------
def plot_network(G, title):
    pos = nx.spring_layout(G, seed=RANDOM_SEED)
    roles = nx.get_node_attributes(G, "role")
    colors = []
    for v in G.nodes:
        if roles.get(v) == "leader":
            colors.append("tab:red")
        elif roles.get(v) == "deputy":
            colors.append("tab:orange")
        else:
            colors.append("tab:blue")

    plt.figure(figsize=(6, 5))
    nx.draw_networkx_nodes(G, pos, node_color=colors, node_size=120, alpha=0.9)
    nx.draw_networkx_edges(G, pos, alpha=0.3)
    plt.title(title)
    plt.axis("off")
    plt.tight_layout()
    plt.show()

def plot_metrics_by_topology(results):
    # barras para métricas chave antes/depois e detecção
    labels = list(results.keys())
    qi_b = [results[k]["qi_before"] for k in labels]
    qi_a = [results[k]["qi_after"] for k in labels]
    rec_gain = [results[k]["recovery_gain"] for k in labels]
    prec = [results[k]["precision"] for k in labels]
    rec = [results[k]["recall"] for k in labels]
    f1 = [results[k]["f1"] for k in labels]
    conn_b = [results[k]["connectivity_before"] for k in labels]
    conn_a = [results[k]["connectivity_after"] for k in labels]
    prop_b = [results[k]["prop_eff_before"] for k in labels]
    prop_a = [results[k]["prop_eff_after"] for k in labels]
    del_b = [results[k]["delivery_before"] for k in labels]
    del_a = [results[k]["delivery_after"] for k in labels]

    x = np.arange(len(labels))

    def bar_pair(y1, y2, title, ylabel):
        w = 0.35
        plt.figure(figsize=(8, 4))
        plt.bar(x - w/2, y1, width=w, label="Antes")
        plt.bar(x + w/2, y2, width=w, label="Depois")
        plt.xticks(x, labels, rotation=10)
        plt.ylabel(ylabel)
        plt.title(title)
        plt.legend()
        plt.tight_layout()
        plt.show()

    bar_pair(qi_b, qi_a, "Índice de Qualidade (QI) - Antes vs Depois", "QI")
    bar_pair(conn_b, conn_a, "Fração do Maior Componente - Antes vs Depois", "Fração")
    bar_pair(prop_b, prop_a, "Eficiência de Propagação - Antes vs Depois", "Eficiência")
    bar_pair(del_b, del_a, "Razão de Entrega (proxy) - Antes vs Depois", "Razão")

    # métricas de detecção
    plt.figure(figsize=(8, 4))
    w = 0.25
    plt.bar(x - w, prec, width=w, label="Precisão")
    plt.bar(x, rec, width=w, label="Recall")
    plt.bar(x + w, f1, width=w, label="F1")
    plt.xticks(x, labels, rotation=10)
    plt.title("Métricas de Detecção por Topologia")
    plt.ylabel("Valor")
    plt.legend()
    plt.tight_layout()
    plt.show()

def print_summary(results):
    for name, r in results.items():
        print(f"\n=== {name} ===")
        print(f"Ação tomada: {r['action']}")
        print(f"QI antes/depois: {r['qi_before']:.3f} -> {r['qi_after']:.3f} (ganho {r['recovery_gain']:.3f})")
        print(f"Conectividade LCC antes/depois: {r['lcc_before']:.2f} -> {r['lcc_after']:.2f}")
        print(f"Eficiência Propagação antes/depois: {r['prop_eff_before']:.3f} -> {r['prop_eff_after']:.3f}")
        print(f"Razão de Entrega antes/depois: {r['delivery_before']:.3f} -> {r['delivery_after']:.3f}")
        print(f"Detecção - Precision: {r['precision']:.2f} | Recall: {r['recall']:.2f} | F1: {r['f1']:.2f}")
        print(f"Ground-truth ataques: {r['ground_truth_attacks']} | Anomalias detectadas: {r['detected_anomalies']}")
        print(f"Autenticação líder/deputy OK? {r['auth_ok']}")

# ---------------------------
# 13) Main
# ---------------------------
if __name__ == "__main__":
    results, graphs = run_all()
    print_summary(results)

    # visualiza as redes após o ciclo
    for name, G in graphs.items():
        plot_network(G, f"{name} - Pós-Reconfiguração")

    # compara métricas entre topologias
    plot_metrics_by_topology(results)