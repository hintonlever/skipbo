import type {
  DatasetMeta,
  GenerationMeta,
  GenerationWeights,
  TrainingConfig,
  TrainingRecord,
  EpochEvent,
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
): () => void {
  const es = new EventSource(`${BASE}/training/stream`);
  es.addEventListener('epoch', (e) => {
    onEpoch(JSON.parse(e.data));
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

// Health
export const healthCheck = () => json<{ ok: boolean }>('/health');
