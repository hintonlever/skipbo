import { useState, useCallback, useRef, useEffect } from 'react';
import { GENERATION_NAMES, type EpochEvent, type GenerationMeta } from './types';
import { LossCurve } from './LossCurve';
import * as api from './api';

interface Props {
  selectedDatasetIds: Set<string>;
  generations: GenerationMeta[];
  onTrainingComplete: () => void;
}

export function TrainingPanel({ selectedDatasetIds, generations, onTrainingComplete }: Props) {
  const existingNames = new Set(generations.map((g) => g.name));
  const nextName = GENERATION_NAMES.find((n) => !existingNames.has(n)) ?? 'Custom';

  const [genName, setGenName] = useState(nextName);
  const [epochs, setEpochs] = useState(50);
  const [lr, setLr] = useState(0.001);
  const [batchSize, setBatchSize] = useState(256);
  const [training, setTraining] = useState(false);
  const [lossData, setLossData] = useState<EpochEvent[]>([]);
  const [currentEpoch, setCurrentEpoch] = useState(0);
  const closeStreamRef = useRef<(() => void) | null>(null);

  // Update suggested name when generations change
  useEffect(() => {
    const existing = new Set(generations.map((g) => g.name));
    const next = GENERATION_NAMES.find((n) => !existing.has(n)) ?? 'Custom';
    setGenName(next);
  }, [generations]);

  const startTraining = useCallback(async () => {
    if (selectedDatasetIds.size === 0) return;

    setTraining(true);
    setLossData([]);
    setCurrentEpoch(0);

    await api.startTraining({
      dataset_ids: Array.from(selectedDatasetIds),
      generation_name: genName,
      epochs,
      batch_size: batchSize,
      learning_rate: lr,
      weight_decay: 0.0001,
      value_weight: 1.0,
      policy_weight: 1.0,
    });

    closeStreamRef.current = api.streamTraining(
      (e) => {
        setLossData((prev) => [...prev, e]);
        setCurrentEpoch(e.epoch);
      },
      () => {
        setTraining(false);
        onTrainingComplete();
      },
      (err) => {
        console.error('Training stream error:', err);
        setTraining(false);
      },
    );
  }, [selectedDatasetIds, genName, epochs, batchSize, lr, onTrainingComplete]);

  const stopTraining = useCallback(async () => {
    await api.stopTraining();
    closeStreamRef.current?.();
    closeStreamRef.current = null;
    setTraining(false);
    onTrainingComplete();
  }, [onTrainingComplete]);

  const lastLoss = lossData.length > 0 ? lossData[lossData.length - 1] : null;

  return (
    <div>
      <h3 style={{ margin: '0 0 12px', fontSize: 15, fontWeight: 600 }}>Training</h3>

      <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap', marginBottom: 12 }}>
        <label style={{ fontSize: 13 }}>
          Generation:
          <input
            value={genName}
            onChange={(e) => setGenName(e.target.value)}
            style={{ marginLeft: 6, width: 120, padding: '2px 6px' }}
            disabled={training}
          />
        </label>
        <label style={{ fontSize: 13 }}>
          Epochs:
          <input type="number" value={epochs} min={1} max={500}
            onChange={(e) => setEpochs(Number(e.target.value))}
            style={{ marginLeft: 6, width: 60, padding: '2px 6px' }}
            disabled={training} />
        </label>
        <label style={{ fontSize: 13 }}>
          LR:
          <input type="number" value={lr} min={0.00001} max={0.1} step={0.0001}
            onChange={(e) => setLr(Number(e.target.value))}
            style={{ marginLeft: 6, width: 80, padding: '2px 6px' }}
            disabled={training} />
        </label>
        <label style={{ fontSize: 13 }}>
          Batch:
          <input type="number" value={batchSize} min={16} max={2048} step={16}
            onChange={(e) => setBatchSize(Number(e.target.value))}
            style={{ marginLeft: 6, width: 70, padding: '2px 6px' }}
            disabled={training} />
        </label>
      </div>

      <div style={{ display: 'flex', gap: 8, marginBottom: 16 }}>
        {!training ? (
          <button
            onClick={startTraining}
            disabled={selectedDatasetIds.size === 0}
            style={{
              padding: '4px 16px', fontSize: 13, fontWeight: 600,
              backgroundColor: selectedDatasetIds.size === 0 ? '#d1d5db' : '#059669',
              color: '#fff', border: 'none', borderRadius: 4,
              cursor: selectedDatasetIds.size === 0 ? 'default' : 'pointer',
            }}>
            Start Training
          </button>
        ) : (
          <button
            onClick={stopTraining}
            style={{
              padding: '4px 16px', fontSize: 13, fontWeight: 600,
              backgroundColor: '#ef4444', color: '#fff',
              border: 'none', borderRadius: 4, cursor: 'pointer',
            }}>
            Stop Training
          </button>
        )}
        {selectedDatasetIds.size === 0 && !training && (
          <span style={{ fontSize: 12, color: '#9ca3af', alignSelf: 'center' }}>
            Select datasets above first
          </span>
        )}
      </div>

      <LossCurve data={lossData} width={500} height={220} />

      {lastLoss && (
        <div style={{ fontSize: 12, color: '#6b7280', marginTop: 6 }}>
          Epoch {currentEpoch}/{epochs}
          {' | '}Policy: {lastLoss.policy_loss.toFixed(4)}
          {' | '}Value: {lastLoss.value_loss.toFixed(4)}
        </div>
      )}
    </div>
  );
}
