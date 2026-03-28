import { useState, useRef, useCallback } from 'react';
import { AI_TYPES, type DatasetMeta, type TrainingRecord } from './types';
import * as api from './api';

interface Props {
  datasets: DatasetMeta[];
  selectedIds: Set<string>;
  onToggle: (id: string) => void;
  onRefresh: () => void;
}

const NUM_WORKERS = Math.min(navigator.hardwareConcurrency || 4, 8);
const UPLOAD_BATCH_SIZE = 500;

export function DatasetPanel({ datasets, selectedIds, onToggle, onRefresh }: Props) {
  const [p0Preset, setP0Preset] = useState(1);
  const [p1Preset, setP1Preset] = useState(1);
  const [numGames, setNumGames] = useState(100);
  const [maxChains, setMaxChains] = useState(50);
  const [datasetName, setDatasetName] = useState('');
  const [generating, setGenerating] = useState(false);
  const [progress, setProgress] = useState({ done: 0, total: 0 });
  const [error, setError] = useState<string | null>(null);
  const workersRef = useRef<Worker[]>([]);

  const getAIConfig = (preset: number) => {
    const ai = AI_TYPES.find((a) => a.value === preset) ?? AI_TYPES[0];
    return { type: ai.type, iters: ai.iters, dets: ai.dets, tree: ai.tree };
  };

  const generate = useCallback(async () => {
    const p0 = getAIConfig(p0Preset);
    const p1 = getAIConfig(p1Preset);
    const p0Label = AI_TYPES.find((a) => a.value === p0Preset)?.label ?? 'Unknown';
    const p1Label = AI_TYPES.find((a) => a.value === p1Preset)?.label ?? 'Unknown';
    const name = datasetName || `${p0Label} vs ${p1Label} ${numGames}g`;

    let meta: DatasetMeta;
    try {
      meta = await api.createDataset({
        name,
        p0_type: p0Label,
        p1_type: p1Label,
        num_games: numGames,
        max_chains_per_turn: maxChains,
      });
    } catch (e) {
      setError(`Failed to create dataset: ${e}. Is the Python server running?`);
      return;
    }

    setGenerating(true);
    setError(null);
    setProgress({ done: 0, total: numGames });

    // Partition games across workers
    const gamesPerWorker = Math.ceil(numGames / NUM_WORKERS);
    let totalDone = 0;
    let pendingRecords: TrainingRecord[] = [];
    let uploading = false;
    const uploadQueue: TrainingRecord[][] = [];

    const uploadBatch = async () => {
      if (uploading) return;
      uploading = true;
      while (uploadQueue.length > 0) {
        const batch = uploadQueue.shift()!;
        try {
          await api.uploadBatch(meta.id, batch);
        } catch (e) {
          console.error('Upload failed:', e);
        }
      }
      uploading = false;
    };

    const flushRecords = () => {
      if (pendingRecords.length >= UPLOAD_BATCH_SIZE) {
        uploadQueue.push(pendingRecords.splice(0, UPLOAD_BATCH_SIZE));
        uploadBatch();
      }
    };

    const workers: Worker[] = [];
    let workersDone = 0;

    for (let w = 0; w < NUM_WORKERS; w++) {
      const startGame = w * gamesPerWorker;
      const count = Math.min(gamesPerWorker, numGames - startGame);
      if (count <= 0) continue;

      const worker = new Worker(new URL('./dataWorker.ts', import.meta.url), { type: 'module' });
      workers.push(worker);

      worker.onmessage = (e) => {
        if (e.data.type === 'gameData') {
          pendingRecords.push(...e.data.records);
          flushRecords();
        } else if (e.data.type === 'progress') {
          totalDone++;
          setProgress({ done: totalDone, total: numGames });
        } else if (e.data.type === 'done') {
          workersDone++;
          if (workersDone === workers.length) {
            // Flush remaining
            if (pendingRecords.length > 0) {
              uploadQueue.push([...pendingRecords]);
              pendingRecords = [];
              uploadBatch().then(() => {
                setGenerating(false);
                onRefresh();
                workers.forEach((w) => w.terminate());
              });
            } else {
              uploadBatch().then(() => {
                setGenerating(false);
                onRefresh();
                workers.forEach((w) => w.terminate());
              });
            }
          }
        } else if (e.data.type === 'error') {
          console.error('Worker error:', e.data.message);
          setError(e.data.message);
          setGenerating(false);
        }
      };

      worker.onerror = (err) => {
        console.error('Worker uncaught error:', err);
        setError(String(err.message || err));
        setGenerating(false);
      };

      worker.postMessage({
        type: 'generate',
        p0Type: p0.type, p0Iters: p0.iters, p0Dets: p0.dets, p0Tree: p0.tree,
        p1Type: p1.type, p1Iters: p1.iters, p1Dets: p1.dets, p1Tree: p1.tree,
        numGames: count,
        maxChains,
        startSeed: (Date.now() % 100000) + startGame * 1000,
      });
    }

    workersRef.current = workers;
  }, [p0Preset, p1Preset, numGames, maxChains, datasetName, onRefresh]);

  const handleDelete = async (id: string) => {
    await api.deleteDataset(id);
    onRefresh();
  };

  const pct = progress.total > 0 ? Math.round((progress.done / progress.total) * 100) : 0;

  return (
    <div>
      <h3 style={{ margin: '0 0 12px', fontSize: 15, fontWeight: 600 }}>Data Generation</h3>
      <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap', marginBottom: 12 }}>
        <label style={{ fontSize: 13 }}>
          Player 0:
          <select value={p0Preset} onChange={(e) => setP0Preset(Number(e.target.value))}
            style={{ marginLeft: 6, padding: '2px 6px' }}>
            {AI_TYPES.map((a) => <option key={a.value} value={a.value}>{a.label}</option>)}
          </select>
        </label>
        <label style={{ fontSize: 13 }}>
          Player 1:
          <select value={p1Preset} onChange={(e) => setP1Preset(Number(e.target.value))}
            style={{ marginLeft: 6, padding: '2px 6px' }}>
            {AI_TYPES.map((a) => <option key={a.value} value={a.value}>{a.label}</option>)}
          </select>
        </label>
        <label style={{ fontSize: 13 }}>
          Games:
          <input type="number" value={numGames} min={1} max={10000}
            onChange={(e) => setNumGames(Number(e.target.value))}
            style={{ marginLeft: 6, width: 70, padding: '2px 6px' }} />
        </label>
        <label style={{ fontSize: 13 }}>
          Max chains:
          <input type="number" value={maxChains} min={5} max={200}
            onChange={(e) => setMaxChains(Number(e.target.value))}
            style={{ marginLeft: 6, width: 60, padding: '2px 6px' }} />
        </label>
      </div>
      <div style={{ display: 'flex', gap: 12, alignItems: 'center', marginBottom: 16 }}>
        <input
          placeholder="Dataset name (auto-generated if empty)"
          value={datasetName}
          onChange={(e) => setDatasetName(e.target.value)}
          style={{ flex: 1, padding: '4px 8px', fontSize: 13 }}
        />
        <button
          onClick={generate}
          disabled={generating}
          style={{
            padding: '4px 16px', fontSize: 13, fontWeight: 600,
            backgroundColor: generating ? '#9ca3af' : '#7c3aed', color: '#fff',
            border: 'none', borderRadius: 4, cursor: generating ? 'default' : 'pointer',
          }}>
          {generating ? `Generating... ${pct}%` : 'Generate Dataset'}
        </button>
      </div>

      {generating && (
        <div style={{ marginBottom: 16 }}>
          <div style={{
            height: 6, backgroundColor: '#e5e7eb', borderRadius: 3, overflow: 'hidden',
          }}>
            <div style={{
              height: '100%', width: `${pct}%`, backgroundColor: '#7c3aed',
              transition: 'width 0.3s',
            }} />
          </div>
          <div style={{ fontSize: 12, color: '#6b7280', marginTop: 4 }}>
            {progress.done} / {progress.total} games
          </div>
        </div>
      )}

      {error && (
        <div style={{
          padding: 10, marginBottom: 12, backgroundColor: '#fef2f2',
          border: '1px solid #fecaca', borderRadius: 4, fontSize: 12, color: '#dc2626',
        }}>
          Error: {error}
        </div>
      )}

      <h3 style={{ margin: '0 0 8px', fontSize: 15, fontWeight: 600 }}>Datasets</h3>
      {datasets.length === 0 ? (
        <div style={{ fontSize: 13, color: '#9ca3af' }}>No datasets yet</div>
      ) : (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
          {datasets.map((ds) => (
            <div key={ds.id} style={{
              display: 'flex', alignItems: 'center', gap: 8, padding: '6px 8px',
              backgroundColor: selectedIds.has(ds.id) ? '#f3f0ff' : '#fff',
              border: '1px solid #e5e7eb', borderRadius: 4, fontSize: 13,
            }}>
              <input
                type="checkbox"
                checked={selectedIds.has(ds.id)}
                onChange={() => onToggle(ds.id)}
              />
              <span style={{ flex: 1, fontWeight: 500 }}>{ds.name}</span>
              <span style={{ color: '#6b7280' }}>{ds.total_turns.toLocaleString()} turns</span>
              <span style={{ color: '#9ca3af', fontSize: 12 }}>{ds.batches} batches</span>
              <button
                onClick={() => handleDelete(ds.id)}
                style={{
                  border: 'none', background: 'none', cursor: 'pointer',
                  color: '#ef4444', fontSize: 14, padding: '0 4px',
                }}>
                x
              </button>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
