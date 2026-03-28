export interface DatasetMeta {
  id: string;
  name: string;
  created: string;
  p0_type: string;
  p1_type: string;
  num_games: number;
  max_chains_per_turn: number;
  total_turns: number;
  batches: number;
}

export interface GenerationMeta {
  name: string;
  value_params: number;
  policy_params: number;
  training_samples: number;
  epochs: number;
  final_policy_loss: number;
  final_value_loss: number;
}

export interface GenerationWeights {
  generation: string;
  value_network: number[];
  policy_network: number[];
  metadata: {
    value_params: number;
    policy_params: number;
    training_samples: number;
    epochs: number;
  };
}

export interface TrainingConfig {
  dataset_ids: string[];
  generation_name: string;
  epochs: number;
  batch_size: number;
  learning_rate: number;
  weight_decay: number;
  value_weight: number;
  policy_weight: number;
}

export interface EpochEvent {
  epoch: number;
  policy_loss: number;
  value_loss: number;
  total_loss: number;
}

export interface TrainingStatus {
  running: boolean;
  generation: string | null;
  epoch: number;
  total_epochs: number;
}

export interface TrainingRecord {
  state: number[];
  chains: number[][];
  chosen: number;
  outcome: number;
}

export const GENERATION_NAMES = [
  'Adelaide', 'Bernard', 'Celeste', 'Dominic', 'Eleanor',
  'Felix', 'Genevieve', 'Hugo', 'Ingrid', 'Julian',
  'Katarina', 'Leonard', 'Marguerite', 'Nathaniel', 'Ophelia', 'Percival',
];

export const AI_TYPES = [
  { value: 0, label: 'Random', type: 0, iters: 0, dets: 0, tree: 0 },
  { value: 1, label: 'Heuristic', type: 1, iters: 0, dets: 0, tree: 0 },
  { value: 3, label: 'Heuristic + Random Discard', type: 3, iters: 0, dets: 0, tree: 0 },
  { value: 10, label: 'MCTS Easy', type: 2, iters: 100, dets: 5, tree: 2 },
  { value: 11, label: 'MCTS Medium', type: 2, iters: 500, dets: 10, tree: 2 },
  { value: 12, label: 'MCTS Hard', type: 2, iters: 2000, dets: 20, tree: 4 },
] as const;
