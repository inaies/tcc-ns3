#!/usr/bin/env python3
"""
agent_isolation.py

Agente para ns3-gym que:
 - coleta features por nó (traffic, packet_rate, latency, energy, ...)
 - faz um warmup (coleta de N passos sem isolar) para aprender comportamento normal
 - treina um IsolationForest (scikit-learn) com dados do warmup
 - em cada passo, prevê anomalias e envia ações de isolamento via env.step(action)
 - faz proteção simples (taxa máxima de isolamentos, cooldown, whitelist do AP, logging)

Como usar (exemplo):
  python3 agent_isolation.py --env-id "Ns3GymEnv-v0" --warmup 30 --contamination 0.05

OBS: Ajuste as chaves de observação / formato conforme seu ns3-gym.
"""

import numpy as np
if not hasattr(np, 'float'):
    np.float = float
import argparse
import time
from sklearn.ensemble import IsolationForest
import gym
import logging

# Tente importar ns3-gym se disponível (opcional)
try:
    import ns3gym  # alguns setups usam este package name
except Exception:
    ns3gym = None

from ns3gym import ns3env

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
logger = logging.getLogger("agent_isolation")

# -----------------------------
# Parâmetros do agente
# -----------------------------
DEFAULT_WARMUP_STEPS = 50         # passos coletados antes de treinar o IsolationForest
DEFAULT_CONTAMINATION = 0.05       # proporção esperada de anomalias
MAX_ISOLATIONS_PER_STEP = 5        # limite de quantos nós isolar por passo
ISOLATION_COOLDOWN = 10            # passos para manter nó isolado antes de poder reativar
MAX_TOTAL_ISOLATIONS = None        # limitador global (None = sem limite)

# -----------------------------
# Funções utilitárias
# -----------------------------
def extract_node_features(obs):
    """
    Adapta-se automaticamente a observações 1D (N,) ou 2D (N, F) do ns3-gym.
    Retorna (X, node_ids).
    """
    import numpy as np

    a = np.array(obs, dtype=float)

    # Caso 2D (N, F)
    if a.ndim == 2:
        return a.copy(), list(range(a.shape[0]))

    # Caso 1D (N,) — só 1 feature por nó
    if a.ndim == 1:
        N = a.size
        F = 1  # uma feature por nó
        a = a.reshape((N, F))
        return a.copy(), list(range(N))

    # Caso dicionário (compatibilidade)
    if isinstance(obs, dict):
        for key in ("node_features", "nodes", "features"):
            if key in obs:
                arr = np.array(obs[key], dtype=float)
                if arr.ndim == 1:
                    N = arr.size
                    return arr.reshape((N, 1)), list(range(N))
                elif arr.ndim == 2:
                    return arr.copy(), list(range(arr.shape[0]))

    raise ValueError(f"Formato de observação não reconhecido: tipo={type(obs)}, shape={getattr(a, 'shape', None)}")


# -----------------------------
# Agente principal
# -----------------------------
class IsolationIsolationAgent:
    def __init__(self, env, warmup_steps=DEFAULT_WARMUP_STEPS,
                 contamination=DEFAULT_CONTAMINATION,
                 max_isolations_per_step=MAX_ISOLATIONS_PER_STEP,
                 isolation_cooldown=ISOLATION_COOLDOWN,
                 max_total_isolations=MAX_TOTAL_ISOLATIONS):
        self.env = env
        self.warmup_steps = warmup_steps
        self.contamination = contamination
        self.max_isolations_per_step = max_isolations_per_step
        self.isolation_cooldown = isolation_cooldown
        self.max_total_isolations = max_total_isolations

        self.model = None
        self.feature_buffer = []  # lista de arrays (N_nodes, F) dos passos de warmup (aplanados)
        self.total_isolated = 0

        # estado de isolamento por nó: dict node_id -> steps_remaining (>=1 se isolado)
        self.isolated_until = {}

        # whitelist opcional (ex: não isolar APs)
        self.whitelist_node_ids = set()  # preencher se quiser ex: {ap_index}

    def warmup_and_train(self):
        """
        Coleta warmup_steps de observações sem tomar ações de isolamento e treina IsolationForest.
        """
        logger.info("Warmup: coletando %d passos para aprender baseline...", self.warmup_steps)
        # Se precisar resetar explicitamente: obs = self.env.reset()
        # Mas assumimos que o loop principal já iniciou o env ou vai pegar o primeiro obs.
        
        # Nota: O loop de warmup precisa interagir com env.step para avançar o tempo
        # Assumindo que o script já recebeu o primeiro obs antes de chamar isso ou vai chamar reset.
        obs = self.env.reset() 

        for step in range(self.warmup_steps):
            X_nodes, node_ids = extract_node_features(obs)
            self.feature_buffer.append(X_nodes)
            
            neutral_action = self.build_neutral_action_for_env(len(node_ids))
            obs, reward, done, info = self.env.step(neutral_action)
            if done:
                logger.info("Env terminou durante warmup (done).")
                break

        # construir dataset
        all_rows = []
        for step_arr in self.feature_buffer:
            for r in step_arr:
                all_rows.append(r)
        X = np.vstack(all_rows)
        
        # --- CORREÇÃO: Adicionar ruído para evitar variância zero ---
        # Como o warmup é todo 0.0, o IsolationForest pode falhar. 
        # Adicionamos um ruído insignificante para criar uma distribuição "normal" em torno de zero.
        noise = np.random.normal(0, 0.01, size=X.shape)
        X_train = X + noise
        # -----------------------------------------------------------

        logger.info("Warmup coletou %d amostras. Treinando com ruído...", X.shape[0])

        self.model = IsolationForest(contamination=self.contamination, random_state=42)
        self.model.fit(X_train)
        logger.info("IsolationForest treinado (contamination=%s).", self.contamination)

    def build_neutral_action_for_env(self, n_nodes):
        """
        Retorna uma ação neutra (sem isolamento). Formatagem depende do env:
        - se action_space é Box shape (n_nodes,) com 0/1: retorna np.zeros(n_nodes)
        - se action_space is Discrete(n): adapt to 0
        ADAPTAR se seu env espera dicionários.
        """
        a_space = getattr(self.env, "action_space", None)
        if a_space is None:
            # fallback: vetor zeros
            return np.zeros(n_nodes, dtype=int)

        try:
            # se Box
            if hasattr(a_space, "shape"):
                shape = a_space.shape
                return np.zeros(shape, dtype=int)
        except Exception:
            pass
        # default
        return np.zeros(n_nodes, dtype=int)

    def step(self, obs, info_str=None, step_idx=None):
        X_nodes, node_ids = extract_node_features(obs)
        N = len(node_ids)

        # --- NOVA LÓGICA DE MAPEAMENTO DE NOMES ---
        node_labels = {}
        if info_str and isinstance(info_str, str):
            try:
                # info_str vem como "0=2001::1|1=2001::2|"
                parts = info_str.strip("|").split("|")
                for p in parts:
                    if "=" in p:
                        idx_s, ip_s = p.split("=")
                        node_labels[int(idx_s)] = ip_s
            except Exception:
                pass # Se falhar, usa o ID numérico mesmo
        # ------------------------------------------

        if self.model is None:
            return self.build_neutral_action_for_env(N)

        try:
            preds = self.model.predict(X_nodes) 
            scores = self.model.decision_function(X_nodes)
        except Exception as e:
            logger.exception("Erro ao prever: %s", e)
            return self.build_neutral_action_for_env(N)

        candidates = []
        for i, nid in enumerate(node_ids):
            # Tenta pegar o IP, senão usa "Node X"
            label = node_labels.get(nid, f"Node {nid}")
            
            candidates.append((nid, int(preds[i]), float(scores[i]), X_nodes[i], label))
            
            traffic_val = X_nodes[i][0]
            if traffic_val > 10000:
                is_anom = "ANOMALIA" if preds[i] == -1 else "Normal"
                # LOG COM ENDEREÇO IP
                logger.info(f"DEBUG [{label}]: Trafego={traffic_val:.1f} Score={scores[i]:.3f} Class={is_anom}")

        # Atualiza lógica para usar a tupla com 5 elementos (adicionamos label)
        anomalous = [c for c in candidates if c[1] == -1 and c[0] not in self.whitelist_node_ids]
        anomalous.sort(key=lambda t: t[2]) 

        # ... (código de cooldown igual) ...
        # (Apenas copie a parte de cooldown do seu código anterior)
        # Gerenciar Cooldown
        to_remove = []
        for nid in list(self.isolated_until.keys()):
            self.isolated_until[nid] -= 1
            if self.isolated_until[nid] <= 0:
                to_remove.append(nid)
        for nid in to_remove:
            del self.isolated_until[nid]
            label = node_labels.get(nid, f"Node {nid}") # Label para o log
            logger.info("%s reativado (cooldown expirado).", label)

        allowed = self.max_isolations_per_step # ... (lógica igual)
        
        # ... (Loop de escolha igual, mas desempacotando label) ...
        chosen = []
        # Atenção aqui: agora candidates tem 5 itens: (nid, pred, score, feat, label)
        for (nid, pred, score, feat, label) in anomalous:
            if allowed <= 0: break
            if nid in self.isolated_until: continue
            
            try:
                traffic = float(feat[0])
            except: 
                traffic = 0
            
            if traffic < 1.0: 
                continue

            chosen.append(nid)
            allowed -= 1
            
            # LOG FINAL DE ISOLAMENTO
            action = np.zeros(N, dtype=int) # Reset action array logic inside loop is wrong in original, assuming valid logic outside
            
        # Recriar o vetor de ação final
        action = np.zeros(N, dtype=int)
        for i, nid in enumerate(node_ids):
            label = node_labels.get(nid, f"Node {nid}")
            if nid in chosen:
                action[i] = 1
                self.isolated_until[nid] = self.isolation_cooldown
                self.total_isolated += 1
                # Encontrar o score correspondente para o log
                sc = 0.0
                for c in candidates: 
                    if c[0] == nid: sc = c[2]; break
                logger.info(">>> ISOLANDO %s (Score=%.4f Traffic=%.1f)", label, sc, float(X_nodes[i][0]))
            else:
                action[i] = 0

        return action

# -----------------------------
# Loop principal
# -----------------------------
def run_agent(env, args):
    agent = IsolationIsolationAgent(env,
                                    warmup_steps=args.warmup,
                                    contamination=args.contamination,
                                    max_isolations_per_step=args.max_isolations,
                                    isolation_cooldown=args.cooldown,
                                    max_total_isolations=args.max_total_isolations)

    agent.warmup_and_train()

    # loop
    obs = None
    done = False
    step_idx = 0
    # reset after warmup: env is already at time after warmup, so we continue from last obs
    # If warmup ended with env.done, call reset
    try:
        # try to get current observation by calling step with neutral action
        neutral = agent.build_neutral_action_for_env(1)  # dummy, will be resized
        obs, reward, done, info = env.step(neutral)
    except Exception:
        # fallback: call reset (some envs expect it)
        obs = env.reset()

    logger.info("Iniciando loop principal do agente...")
    while not done:
        current_info = info if 'info' in locals() else None
        action = agent.step(obs, step_idx=step_idx)
        # step in the env
        obs, reward, done, info = env.step(action)
        step_idx += 1
        # logging básico
        if step_idx % 10 == 0:
            logger.info("Step %d done=%s reward=%s", step_idx, done, reward)
    logger.info("Env terminou. Agent steps: %d", step_idx)

# -----------------------------
# CLI e execução
# -----------------------------
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--env-id", type=str, default=None,
                        help="ID do ambiente ns3-gym (ou caminho para wrapper). Ex: 'ns3gym-v0' ou None para usar gym.make")
    parser.add_argument("--warmup", type=int, default=DEFAULT_WARMUP_STEPS)
    parser.add_argument("--contamination", type=float, default=DEFAULT_CONTAMINATION)
    parser.add_argument("--max-isolations", type=int, default=MAX_ISOLATIONS_PER_STEP)
    parser.add_argument("--cooldown", type=int, default=ISOLATION_COOLDOWN)
    parser.add_argument("--max-total-isolations", type=int, default=None)
    args = parser.parse_args()

    # Cria ambiente: tenta gym.make, senão usa ns3gym wrapper se disponível
    env = None
    if args.env_id:
        try:
            env = gym.make(args.env_id)
            logger.info("Ambiente criado via gym.make('%s')", args.env_id)
        except Exception as e:
            logger.warning("gym.make falhou: %s", e)
    if env is None and ns3gym is not None:
        try:
            # ADAPTAR: se o seu ns3-gym fornece uma factory diferente, mude aqui
            env = ns3env.Ns3Env(
                port=5555,        # mesma porta usada no seu ResilientEnv
                stepTime=0.5,     # intervalo de decisão entre passos
                startSim=False,   # True se quiser que o NS-3 inicie junto
                simSeed=0,
                simArgs={},
                debug=False)            
            logger.info("Ambiente criado via ns3gym.RLEnvironment()")
        except Exception as e:
            logger.exception("Falha ao criar ambiente via ns3gym: %s", e)
            raise

    if env is None:
        raise RuntimeError("Não foi possível criar o ambiente ns3-gym. Ajuste --env-id ou instale ns3gym.")

    run_agent(env, args)

if __name__ == "__main__":
    main()
