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
import pandas as pd

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
DEFAULT_WARMUP_STEPS = 50        # passos coletados antes de treinar o IsolationForest
DEFAULT_CONTAMINATION = 0.05      # proporção esperada de anomalias
MAX_ISOLATIONS_PER_STEP = 5        # limite de quantos nós isolar por passo
ISOLATION_COOLDOWN = 10            # passos para manter nó isolado antes de poder reativar
MAX_TOTAL_ISOLATIONS = None        # limitador global (None = sem limite)

# --- A GRANDE PROTEÇÃO: THRESHOLD SEGURO ---
# Tráfego legítimo ronda os 2.000 Bytes/s. Ataque ronda os 12.000.000 Bytes/s.
# Só isolamos se o nó enviar mais de 500.000 Bytes/s.
THRESHOLD_SEGURO = 500000.0 
# -------------------------------------------

# -----------------------------
# Funções utilitárias
# -----------------------------
def extract_node_features(obs):
    """
    Adapta-se automaticamente a observações 1D (N,) ou 2D (N, F) do ns3-gym.
    Retorna (X, node_ids).
    """
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
        
        # Adicionar ruído para evitar variância zero
        noise = np.random.normal(0, 0.01, size=X.shape)
        X_train = X + noise

        logger.info("Warmup coletou %d amostras. Treinando com ruído...", X.shape[0])

        self.model = IsolationForest(contamination=self.contamination, random_state=42)
        self.model.fit(X_train)
        logger.info("IsolationForest treinado (contamination=%s).", self.contamination)

    def train_with_weather_dataset(self, dataset_path):
        """
        Lê o dataset Train_Test_IoT_weather.csv e treina o modelo.
        Converte o packet_rate para bytes_per_sec multiplicando pelo tamanho do pacote (1024).
        """
        logger.info(f"A tentar carregar o dataset IoT Weather: {dataset_path}...")
        
        try:
            import pandas as pd
            df = pd.read_csv(dataset_path)
            
            if 'label' in df.columns:
                df_normal = df[df['label'] == 0].copy()
            elif 'type' in df.columns:
                df_normal = df[df['type'] == 'normal'].copy()
            else:
                logger.error("Colunas de classificação não encontradas!")
                return False

            if len(df_normal) == 0:
                logger.warning("Nenhum tráfego 'normal' encontrado. A usar tudo.")
                df_normal = df.copy()

            if 'packet_rate' in df_normal.columns:
                df_normal['bytes_per_sec'] = df_normal['packet_rate'] * 1024.0
            else:
                logger.error("Coluna 'packet_rate' não encontrada!")
                return False

            X_train = df_normal[['bytes_per_sec']].values
            
            logger.info("A treinar a IA com %d amostras de telemetria IoT real...", X_train.shape[0])

            self.model = IsolationForest(contamination=self.contamination, random_state=42)
            self.model.fit(X_train)
            
            media_bytes = df_normal['bytes_per_sec'].mean()
            logger.info(f"Modelo TREINADO COM SUCESSO! Média de tráfego legítimo: {media_bytes:.2f} Bytes/s")
            
            return True
            
        except FileNotFoundError:
            logger.error(f"Ficheiro não encontrado: {dataset_path}. Vai usar o warmup padrão.")
            return False
        except Exception as e:
            logger.exception("Erro ao carregar o dataset: %s", e)
            return False

    def build_neutral_action_for_env(self, n_nodes):
        a_space = getattr(self.env, "action_space", None)
        if a_space is None:
            return np.zeros(n_nodes, dtype=int)
        try:
            if hasattr(a_space, "shape"):
                shape = a_space.shape
                return np.zeros(shape, dtype=int)
        except Exception:
            pass
        return np.zeros(n_nodes, dtype=int)

    def step(self, obs, info_str=None, step_idx=None):
        X_nodes, node_ids = extract_node_features(obs)
        N = len(node_ids)

        # Mapeamento de nomes IP
        node_labels = {}
        if info_str and isinstance(info_str, str):
            try:
                parts = info_str.strip("|").split("|")
                for p in parts:
                    if "=" in p:
                        idx_s, ip_s = p.split("=")
                        node_labels[int(idx_s)] = ip_s
            except Exception:
                pass

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
            label = node_labels.get(nid, f"Node {nid}")
            candidates.append((nid, int(preds[i]), float(scores[i]), X_nodes[i], label))
            
            traffic_val = X_nodes[i][0]
            if traffic_val > 10000:
                is_anom = "ANOMALIA" if preds[i] == -1 else "Normal"
                logger.info(f"DEBUG [{label}]: Trafego={traffic_val:.1f} Score={scores[i]:.3f} Class={is_anom}")

        anomalous = [c for c in candidates if c[1] == -1 and c[0] not in self.whitelist_node_ids]
        anomalous.sort(key=lambda t: t[2]) 

        # Gerenciar Cooldown
        to_remove = []
        for nid in list(self.isolated_until.keys()):
            self.isolated_until[nid] -= 1
            if self.isolated_until[nid] <= 0:
                to_remove.append(nid)
        for nid in to_remove:
            del self.isolated_until[nid]
            label = node_labels.get(nid, f"Node {nid}") 
            logger.info("%s reativado (cooldown expirado).", label)

        allowed = self.max_isolations_per_step 
        
        chosen = []
        for (nid, pred, score, feat, label) in anomalous:
            if allowed <= 0: break
            if nid in self.isolated_until: continue
            
            try:
                traffic = float(feat[0])
            except: 
                traffic = 0
            
            # --- PROTEÇÃO CONTRA FALSOS POSITIVOS ---
            if traffic < THRESHOLD_SEGURO: 
                # Se o tráfego for menor que 50.000 Bytes/s, ignorar (proteger tráfego comum)
                continue

            chosen.append(nid)
            allowed -= 1
            
        # Recriar o vetor de ação final
        action = np.zeros(N, dtype=int)
        for i, nid in enumerate(node_ids):
            label = node_labels.get(nid, f"Node {nid}")
            if nid in chosen:
                action[i] = 1
                self.isolated_until[nid] = self.isolation_cooldown
                self.total_isolated += 1
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

    if args.dataset:
        sucesso = agent.train_with_weather_dataset(args.dataset)
        if not sucesso:
            agent.warmup_and_train()
    else:
        agent.warmup_and_train()

    obs = None
    done = False
    step_idx = 0
    try:
        neutral = agent.build_neutral_action_for_env(1) 
        obs, reward, done, info = env.step(neutral)
    except Exception:
        obs = env.reset()

    logger.info("Iniciando loop principal do agente...")
    while not done:
        current_info = info if 'info' in locals() else None
        action = agent.step(obs, info_str=current_info, step_idx=step_idx) # Passei info_str corrigido
        obs, reward, done, info = env.step(action)
        step_idx += 1
        if step_idx % 10 == 0:
            logger.info("Step %d done=%s reward=%s", step_idx, done, reward)
    logger.info("Env terminou. Agent steps: %d", step_idx)

# -----------------------------
# CLI e execução
# -----------------------------
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--env-id", type=str, default=None, help="ID do ambiente ns3-gym")
    parser.add_argument("--dataset", type=str, default=None, help="Caminho para o ficheiro CSV")
    parser.add_argument("--warmup", type=int, default=150)
    parser.add_argument("--contamination", type=float, default=0.1)
    parser.add_argument("--max-isolations", type=int, default=5)
    parser.add_argument("--cooldown", type=int, default=10)
    parser.add_argument("--max-total-isolations", type=int, default=10)
    
    args = parser.parse_args()

    env = None
    if args.env_id:
        try:
            env = gym.make(args.env_id)
            logger.info("Ambiente criado via gym.make('%s')", args.env_id)
        except Exception as e:
            logger.warning("gym.make falhou: %s", e)
    if env is None and ns3gym is not None:
        try:
            env = ns3env.Ns3Env(
                port=5555,
                stepTime=0.5,
                startSim=False,
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