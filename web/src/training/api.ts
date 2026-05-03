import type {
  DatasetMeta,
  GenerationMeta,
  GenerationWeights,
  TrainingConfig,
  TrainingRecord,
  EpochEvent,
  DatasetStatsEvent,
  SanityStatsEvent,
} from './types';

const BASE = '/api';

async function json<T>(url: string, init?: RequestInit): Promise<T> {
  const res = await fetch(`${BASE}${url}`, init);
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
}

// Datasets
export const listDatasets = () => json<DatasetMeta[]>('/datasets');

export const createDataset = (meta: {
  name: string;
  p0_type: string;
  p1_type: string;
  num_games: number;
  max_chains_per_turn: number;
}) =>
  json<DatasetMeta>('/datasets', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(meta),
  });

export const deleteDataset = (id: string) =>
  json('/datasets/' + id, { method: 'DELETE' });

export const uploadBatch = (id: string, records: TrainingRecord[]) =>
  json('/datasets/' + id + '/upload', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ records }),
  });

// Training
export const startTraining = (config: TrainingConfig) =>
  json('/training/start', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(config),
  });

export const stopTraining = () =>
  json('/training/stop', { method: 'POST' });

export const getTrainingStatus = () =>
  json<{ running: boolean; generation: string | null; epoch: number; total_epochs: number }>(
    '/training/status'
  );

export function streamTraining(
  onEpoch: (e: EpochEvent) => void,
  onDone: () => void,
  onError: (err: string) => void,
  onDatasetStats?: (e: DatasetStatsEvent) => void,
  onSanityStats?: (e: SanityStatsEvent) => void,
): () => void {
  const es = new EventSource(`${BASE}/training/stream`);
  es.addEventListener('epoch', (e) => {
    onEpoch(JSON.parse(e.data));
  });
  es.addEventListener('dataset_stats', (e) => {
    onDatasetStats?.(JSON.parse(e.data));
  });
  es.addEventListener('sanity_stats', (e) => {
    onSanityStats?.(JSON.parse(e.data));
  });
  es.addEventListener('done', () => {
    onDone();
    es.close();
  });
  es.onerror = () => {
    onError('SSE connection error');
    es.close();
  };
  return () => es.close();
}

// Generations
export const listGenerations = () => json<GenerationMeta[]>('/generations');

export const getWeights = (name: string) =>
  json<GenerationWeights>('/generations/' + name + '/weights');

export const deleteGeneration = (name: string) =>
  json('/generations/' + name, { method: 'DELETE' });

// PPO Training
export interface PPOConfig {
  generation_name: string;
  num_games: number;
  num_batches: number;
  lr: number;
  ppo_epochs: number;
  minibatch_size: number;
  gamma: number;
  gae_lambda: number;
  clip_epsilon: number;
  entropy_coef: number;
  opponent: string;
  device: string;
}

export interface PPOBatchEvent {
  batch: number;
  win_rate: number;
  steps: number;
  avg_game_length: number;
  pg_loss: number;
  v_loss: number;
  entropy: number;
  elapsed: number;
}

export const startPPO = (config: PPOConfig) =>
  json('/ppo/start', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(config),
  });

export const stopPPO = () =>
  json('/ppo/stop', { method: 'POST' });

export const getPPOStatus = () =>
  json<{ running: boolean; batch: number; total_batches: number; generation: string | null }>(
    '/ppo/status'
  );

export function streamPPO(
  onBatch: (e: PPOBatchEvent) => void,
  onInfo: (msg: string) => void,
  onDone: () => void,
  onError: (err: string) => void,
): () => void {
  const es = new EventSource(`${BASE}/ppo/stream`);
  es.addEventListener('batch', (e) => { onBatch(JSON.parse(e.data)); });
  es.addEventListener('info', (e) => { onInfo(JSON.parse(e.data).message); });
  es.addEventListener('done', () => { onDone(); es.close(); });
  es.addEventListener('error', (e) => {
    try { onError(JSON.parse((e as MessageEvent).data).message); } catch { /* ignore */ }
  });
  es.addEventListener('finished', () => { onDone(); es.close(); });
  es.onerror = () => { onError('SSE connection error'); es.close(); };
  return () => es.close();
}

// Health
export const healthCheck = () => json<{ ok: boolean }>('/health');
