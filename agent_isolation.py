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
import csv
import os

# Cria/Limpa o ficheiro de métricas da IA
with open('metricas_ia.csv', mode='w', newline='') as file:
    writer = csv.writer(file)
    writer.writerow(['Step_Segundo', 'Anomalias_Detetadas', 'Nos_Isolados_Total', 'Score_Mais_Perigoso', 'Score_Medio_Rede'])

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
DEFAULT_WARMUP_STEPS = 150        # passos coletados antes de treinar o IsolationForest
DEFAULT_CONTAMINATION = 0.05      # proporção esperada de anomalias
MAX_ISOLATIONS_PER_STEP = 5        # limite de quantos nós isolar por passo
ISOLATION_COOLDOWN = 10            # passos para manter nó isolado antes de poder reativar
MAX_TOTAL_ISOLATIONS = None        # limitador global (None = sem limite)

# -----------------------------
# Funções utilitárias
# -----------------------------

# Transforma a observação do ambiente em uma matriz de features (N_nodes, F) e uma lista de node_ids
def extract_node_features(obs):

    # Adapta-se automaticamente a observações 1D (N,) ou 2D (N, F) do ns3-gym.    
    # Retorna (X, node_ids).
    a = np.array(obs, dtype=float)

    # Caso 2D (N, F)
    if a.ndim == 2:
        return a.copy(), list(range(a.shape[0]))

    # Caso 1D (N,) — só 1 feature por nó
    # Retorna uma matriz de uma coluna só
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
        self.idle_steps = 15     # Ignora os primeiros 15s (fase de arranque/rampa)
        self.warmup_steps = warmup_steps
        self.contamination = contamination
        self.max_isolations_per_step = max_isolations_per_step
        self.isolation_cooldown = isolation_cooldown
        self.max_total_isolations = max_total_isolations

        self.model = None
        self.feature_buffer = []  # Lista de arrays (N_nodes, F) dos passos de warmup (aplanados)
        self.total_isolated = 0

        # Estado de isolamento por nó: dict node_id -> steps_remaining (>=1 se isolado)
        self.isolated_until = {}

        # Whitelist opcional (ex: não isolar APs)
        self.whitelist_node_ids = set()  # Preencher se quiser ex: {ap_index}

    def warmup_and_train(self):
        logger.info("Iniciando a simulação... Aguardando %d passos para a rede estabilizar.", self.idle_steps)
        obs = self.env.reset() 

        # Fase de espera - Ignora os dados iniciais enquanto os nós acordam
        for step in range(self.idle_steps):
            _, node_ids = extract_node_features(obs)
            # Gera uma ação neutra - array de zeros
            neutral_action = self.build_neutral_action_for_env(len(node_ids))

            # Envia uma ação neutra (sem isolamento)
            obs, reward, done, info = self.env.step(neutral_action)
            if done: return

        # Fase de treino - Capturar dados limpos da rede estabilizada
        logger.info("Rede estabilizada! Coletando %d passos para treinar a IA...", self.warmup_steps)
        for step in range(self.warmup_steps):
            # Armazena as features de cada nó para treinar o modelo depois
            X_nodes, node_ids = extract_node_features(obs)
            self.feature_buffer.append(X_nodes)
            
            # Envia uma ação neutra (sem isolamento) durante o warmup
            neutral_action = self.build_neutral_action_for_env(len(node_ids))
            obs, reward, done, info = self.env.step(neutral_action)
            if done: return

        # Construi dataset e treina o Isolation Forest
        all_rows = []
        for step_arr in self.feature_buffer:
            for r in step_arr:
                all_rows.append(r)
        X = np.vstack(all_rows)
        
        # Calcula a MÉDIA (e não o máximo) com base nos dados coletados do tráfego comum
        self.mean_normal_traffic = float(np.mean(X))
        self.std_normal_traffic = float(np.std(X))
        self.limite_seguranca = self.mean_normal_traffic + (3 * self.std_normal_traffic)
        
        logger.info("Baseline -> Média: %.2f | Limite de Segurança (3-Sigma): %.2f Bytes/s", 
                    self.mean_normal_traffic, self.limite_seguranca)
        noise = np.random.normal(0, 0.01, size=X.shape)
        X_train = X + noise

        self.model = IsolationForest(contamination=self.contamination, random_state=42)
        self.model.fit(X_train)
        logger.info("IA Treinada com sucesso! 100% Autônoma.")

    def train_with_weather_dataset(self, dataset_path):
        # Lê o dataset Train_Test_IoT_weather.csv e treina o modelo.
        # Converte o packet_rate para bytes_per_sec multiplicando pelo tamanho do pacote (1024).

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
            
            self.mean_normal_traffic = float(df_normal['bytes_per_sec'].mean())
            self.std_normal_traffic = float(df_normal['bytes_per_sec'].std())
            self.limite_seguranca = self.mean_normal_traffic + (3 * self.std_normal_traffic)
            logger.info(f"Modelo TREINADO COM SUCESSO! Média: {self.mean_normal_traffic:.2f} | Limite (3-Sigma): {self.limite_seguranca:.2f} Bytes/s")

            return True
            
        except FileNotFoundError:
            logger.error(f"Ficheiro não encontrado: {dataset_path}. Vai usar o warmup padrão.")
            return False
        except Exception as e:
            logger.exception("Erro ao carregar o dataset: %s", e)
            return False

    # Constrói uma ação neutra (sem isolamento) adaptada ao formato do ambiente
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

    # Recebe a observação, faz a previsão de anomalias e retorna a ação de isolamento
    def step(self, obs, info_str=None, step_idx=None):
        # Extrai as features dos nós e seus IDs
        X_nodes, node_ids = extract_node_features(obs)
        N = len(node_ids)

        # Mapeamento de nomes IP
        node_labels = {}
        if info_str and isinstance(info_str, str):
            try:
                # Remove a barra | da string com os IPs, transforma numa lista
                parts = info_str.strip("|").split("|")
                # Mapeia cada parte "idx=ip" para o dicionário node_labels
                for p in parts:
                    if "=" in p:
                        idx_s, ip_s = p.split("=")
                        node_labels[int(idx_s)] = ip_s
            # Caso em que a string vem corrompida é ignorado no mapeamento, usando o ID numérico
            except Exception:
                pass

        # Caso a IA não tenha sido treinada (ex: falha no dataset), retorna ação neutra para evitar erros
        if self.model is None:
            return self.build_neutral_action_for_env(N)

        # Previsão de anomalias e scores para cada nó
        try:
            preds = self.model.predict(X_nodes) 
            # Devolve score da anomalia (quanto mais negativo, mais anômalo)
            scores = self.model.decision_function(X_nodes)
        except Exception as e:
            logger.exception("Erro ao prever: %s", e)
            return self.build_neutral_action_for_env(N)

        # Preenche a lista de nós com info sobre a previsão e score para filtragem e logging
        candidates = []
        for i, nid in enumerate(node_ids):
            label = node_labels.get(nid, f"Node {nid}")
            candidates.append((nid, int(preds[i]), float(scores[i]), X_nodes[i], label))
            
            traffic_val = X_nodes[i][0]
            if traffic_val > 10000:
                is_anom = "ANOMALIA" if preds[i] == -1 else "Normal"
                logger.info(f"DEBUG [{label}]: Trafego={traffic_val:.1f} Score={scores[i]:.3f} Class={is_anom}")

        # Gerencimento do Cooldown
        to_remove = []
        # Percorre lista de nós isolados e decrementa o contador de cooldown
        for nid in list(self.isolated_until.keys()):
            self.isolated_until[nid] -= 1
            # Tempo de isolamento (20s) expirou, nó é adicionado a lista de reativação
            if self.isolated_until[nid] <= 0:
                to_remove.append(nid)
        # Remove os nós que expiraram do isolamento
        for nid in to_remove:
            del self.isolated_until[nid]
            label = node_labels.get(nid, f"Node {nid}") 
            logger.info("%s reativado (cooldown expirado).", label)

        # Defini limite de isolamentos
        allowed = self.max_isolations_per_step 

        # Filtragem dupla
        anomalous = [c for c in candidates if c[1] == -1 and c[0] not in self.whitelist_node_ids]
        # Ordena anomalias pelo score para priorizar os piores casos
        anomalous.sort(key=lambda t: t[2]) 

        # Isolamento dos nós
        chosen = []
        for (nid, pred, score, feat, label) in anomalous:
            # Checa o limit de isolamentos por passo
            if allowed <= 0: break
            # Checa nó já bloqueado
            if nid in self.isolated_until: continue
            
            traffic_bytes = float(feat[0])
            
            # Escuo estatístico (3-SIGMA):
            # Comparação do tráfego com o limite superior estatístico da rede normal.
            if traffic_bytes > self.limite_seguranca:
                chosen.append(nid)
                allowed -= 1
            
        # Executa ação de isolamento
        action = np.zeros(N, dtype=int)
        for i, nid in enumerate(node_ids):
            label = node_labels.get(nid, f"Node {nid}")
            
            # Verifica nó isolado ou escolhido para isolamento
            if nid in chosen or nid in self.isolated_until:
                # Ação de isolamento
                action[i] = 1
                
                # Verifica nó escolhido para isolamento nesse passo
                if nid in chosen:
                    # Marca início do isolamento de 20s
                    self.isolated_until[nid] = self.isolation_cooldown
                    self.total_isolated += 1
                    sc = 0.0
                    for c in candidates: 
                        if c[0] == nid: sc = c[2]; break
                    logger.info(">>> ISOLANDO %s (Score=%.4f Traffic=%.1f)", label, sc, float(X_nodes[i][0]))
            else:
                action[i] = 0

        # Calcular estatísticas do score
        score_mais_perigoso = float(np.min(scores)) if len(scores) > 0 else 0.0
        score_medio = float(np.mean(scores)) if len(scores) > 0 else 0.0
        total_anomalias_detetadas = len(anomalous)
        nos_isolados_neste_momento = len(self.isolated_until)

        return action, total_anomalias_detetadas, nos_isolados_neste_momento, score_mais_perigoso, score_medio

# Loop principal
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
    # Primeira ação neutra para inciar o loop e obter obs
    try:
        neutral = agent.build_neutral_action_for_env(1) 
        obs, reward, done, info = env.step(neutral)
    except Exception:
        obs = env.reset()

    logger.info("Iniciando loop principal do agente...")
    while not done:
        current_info = info if 'info' in locals() else None
        
        # Recebe a ação e as métricas da IA
        action, anom_detetadas, nos_isol, score_max, score_med = agent.step(obs, info_str=current_info, step_idx=step_idx) 
        
        # Envia a ação para o ns-3
        obs, reward, done, info = env.step(action)
        
        # Grava os dados da IA no CSV
        with open('metricas_ia.csv', mode='a', newline='') as file:
            writer = csv.writer(file)
            writer.writerow([step_idx, anom_detetadas, nos_isol, score_max, score_med])
            
        step_idx += 1
        if step_idx % 10 == 0:
            logger.info("Step %d done=%s. Isolados: %d | Max Score: %.3f", step_idx, done, nos_isol, score_max)
            
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
    parser.add_argument("--cooldown", type=int, default=20)
    parser.add_argument("--max-total-isolations", type=int, default=20)
    
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