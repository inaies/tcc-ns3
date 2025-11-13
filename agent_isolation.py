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
DEFAULT_WARMUP_STEPS = 30          # passos coletados antes de treinar o IsolationForest
DEFAULT_CONTAMINATION = 0.05       # proporção esperada de anomalias
MAX_ISOLATIONS_PER_STEP = 5        # limite de quantos nós isolar por passo
ISOLATION_COOLDOWN = 10            # passos para manter nó isolado antes de poder reativar
MAX_TOTAL_ISOLATIONS = None        # limitador global (None = sem limite)

# -----------------------------
# Funções utilitárias
# -----------------------------
def extract_node_features(obs):
    """
    Extrai array (N_nodes, 4) de features a partir da observação recebida do ns-3.
    O ambiente ns3-gym envia um vetor 1D flatten (N_nodes * 4) de floats.
    Retorna (X, node_ids).
    """
    import numpy as np

    # Converte obs para numpy array
    a = np.array(obs, dtype=float)

    # Caso direto: já vem 2D (N, 4)
    if a.ndim == 2 and a.shape[1] == 4:
        return a.copy(), list(range(a.shape[0]))

    # Caso flatten 1D (N * 4)
    if a.ndim == 1:
        F = 4  # features por nó (throughput, delay, loss, queue)
        if a.size % F == 0:
            N = a.size // F
            a = a.reshape((N, F))
            return a.copy(), list(range(N))

    # Caso dicionário com lista linear
    if isinstance(obs, dict):
        for key in ("node_features", "nodes", "features"):
            if key in obs:
                arr = np.array(obs[key], dtype=float)
                if arr.ndim == 1 and arr.size % 4 == 0:
                    N = arr.size // 4
                    arr = arr.reshape((N, 4))
                    return arr.copy(), list(range(N))
                elif arr.ndim == 2 and arr.shape[1] == 4:
                    return arr.copy(), list(range(arr.shape[0]))

    # Caso não identificado
    raise ValueError(
        f"Formato de observação não reconhecido: tipo={type(obs)}, shape={getattr(a, 'shape', None)}"
    )

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
            # extrai features por nó
            X_nodes, node_ids = extract_node_features(obs)
            # aplanar: concatenar todas as linhas para formar um vetor representando o passo
            # alternativa: treinar por nó (aqui usamos vetores por nó agregados)
            self.feature_buffer.append(X_nodes)  # guardamos por passo
            # pass-through action: nenhuma ação (assumimos que env aceita zeros)
            # montar ação neutra conforme a action_space
            neutral_action = self.build_neutral_action_for_env(len(node_ids))
            obs, reward, done, info = self.env.step(neutral_action)
            if done:
                logger.info("Env terminou durante warmup (done).")
                break

        # construir dataset de amostras por nó (cada linha = features de um nó no tempo)
        all_rows = []
        for step_arr in self.feature_buffer:
            for r in step_arr:
                all_rows.append(r)
        X = np.vstack(all_rows)
        logger.info("Warmup coletou %d amostras (por-nó) para treinar IsolationForest.", X.shape[0])

        # treinar IsolationForest
        self.model = IsolationForest(contamination=self.contamination, random_state=42)
        self.model.fit(X)
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

    def step(self, obs, step_idx=None):
        """
        Recebe obs, detecta anomalias, decide quais nós isolar e retorna action.
        """
        X_nodes, node_ids = extract_node_features(obs)
        N = len(node_ids)
        if self.model is None:
            # sem modelo, ação neutra
            return self.build_neutral_action_for_env(N)

        # prever anomalias por nó: predict retorna 1 (normal) e -1 (anomalia)
        try:
            preds = self.model.predict(X_nodes)
            # decision_function -> score (quanto maior, menos anomalia). Usamos scores para desempate.
            scores = self.model.decision_function(X_nodes)
        except Exception as e:
            logger.exception("Erro ao prever com IsolationForest: %s", e)
            return self.build_neutral_action_for_env(N)

        # construir lista de (node_id, pred, score)
        candidates = []
        for i, nid in enumerate(node_ids):
            candidates.append((nid, int(preds[i]), float(scores[i]), X_nodes[i]))

        # filtrar por pred==-1 (anomalia) e não estar na whitelist e estar vivo
        anomalous = [c for c in candidates if c[1] == -1 and c[0] not in self.whitelist_node_ids]

        # ordenar por score (mais negativo = mais anomalous -> filas decrescentes)
        anomalous.sort(key=lambda t: t[2])  # ascending: menor score (mais anômalo) primeiro

        # aplicar cooldown: nós já isolados não sejam re-isolados, e decrementa timers
        # decrementa timers
        to_remove = []
        for nid in list(self.isolated_until.keys()):
            self.isolated_until[nid] -= 1
            if self.isolated_until[nid] <= 0:
                to_remove.append(nid)
        for nid in to_remove:
            del self.isolated_until[nid]
            logger.info("Node %d reativado (cooldown expirado).", nid)

        # decidir quantos isolar (respeitar limites)
        allowed = self.max_isolations_per_step
        if self.max_total_isolations is not None:
            allowed = min(allowed, max(0, (self.max_total_isolations - self.total_isolated)))
        chosen = []
        for (nid, pred, score, feat) in anomalous:
            if allowed <= 0:
                break
            if nid in self.isolated_until:
                continue
            # adicional: verificar feature heuristics (por ex: tráfego muito alto) antes de isolar
            # aqui assumimos coluna 0=traffic, 2=latency (ajuste conforme seu formato)
            try:
                traffic = float(feat[0])
            except Exception:
                traffic = None
            # Exemplo de regra adicional: exigir tráfego > threshold (evita falso positivo)
            if traffic is not None and traffic < 1.0:
                # ignora anomalias fracas
                continue
            chosen.append(nid)
            allowed -= 1

        # montar vetor de ação: 1 = isolar, 0 = manter
        action = np.zeros(N, dtype=int)
        for i, nid in enumerate(node_ids):
            if nid in chosen:
                action[i] = 1
                # marca cooldown
                self.isolated_until[nid] = self.isolation_cooldown
                self.total_isolated += 1
                logger.info("Isolando node %d (score=%.4f).", nid, dict(((c[0], c[2]) for c in anomalous)).get(nid, 0.0))
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
