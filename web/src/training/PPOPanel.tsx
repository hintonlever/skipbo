import { useState, useCallback, useRef } from 'react';
import * as api from './api';
import type { PPOBatchEvent } from './api';

export function PPOPanel({ onDone }: { onDone: () => void }) {
  const [genName, setGenName] = useState('ppo-v1');
  const [numGames, setNumGames] = useState(256);
  const [numBatches, setNumBatches] = useState(200);
  const [opponent, setOpponent] = useState('self');
  const [device, setDevice] = useState('cpu');
  const [lr, setLr] = useState(0.0003);
  const [entropyCoef, setEntropyCoef] = useState(0.01);

  const [running, setRunning] = useState(false);
  const [batches, setBatches] = useState<PPOBatchEvent[]>([]);
  const [info, setInfo] = useState('');
  const [error, setError] = useState('');
  const stopStream = useRef<(() => void) | null>(null);

  const start = useCallback(async () => {
    setError('');
    setBatches([]);
    setInfo('');
    try {
      await api.startPPO({
        generation_name: genName,
        num_games: numGames,
        num_batches: numBatches,
        lr,
        ppo_epochs: 4,
        minibatch_size: 512,
        gamma: 0.99,
        gae_lambda: 0.95,
        clip_epsilon: 0.2,
        entropy_coef: entropyCoef,
        opponent,
        device,
      });
      setRunning(true);

      stopStream.current = api.streamPPO(
        (e) => setBatches((prev) => [...prev, e]),
        (msg) => setInfo(msg),
        () => { setRunning(false); onDone(); },
        (err) => { setError(err); setRunning(false); },
      );
    } catch (e) {
      setError(String(e));
    }
  }, [genName, numGames, numBatches, lr, entropyCoef, opponent, device, onDone]);

  const stop = useCallback(async () => {
    try {
      await api.stopPPO();
    } catch { /* ignore */ }
    stopStream.current?.();
    setRunning(false);
  }, []);

  const lastBatch = batches[batches.length - 1];
  const inputStyle = {
    width: 80, padding: '4px 8px', borderRadius: 4,
    border: '1px solid #d1d5db', fontSize: 13,
  };
  const labelStyle = { fontSize: 12, color: '#374151', fontWeight: 500 as const };

  return (
    <div>
      <h3 style={{ fontSize: 16, fontWeight: 600, marginBottom: 12 }}>PPO Training</h3>

      {/* Config */}
      <div style={{
        display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(180px, 1fr))',
        gap: 12, marginBottom: 16, padding: 16,
        backgroundColor: '#f9fafb', borderRadius: 8,
      }}>
        <label style={labelStyle}>
          Generation name
          <input value={genName} onChange={e => setGenName(e.target.value)}
            disabled={running} style={{ ...inputStyle, width: '100%', display: 'block', marginTop: 4 }} />
        </label>
        <label style={labelStyle}>
          Games/batch
          <input type="number" min={8} max={2048} value={numGames}
            onChange={e => setNumGames(Number(e.target.value))}
            disabled={running} style={{ ...inputStyle, display: 'block', marginTop: 4 }} />
        </label>
        <label style={labelStyle}>
          Batches
          <input type="number" min={10} max={5000} value={numBatches}
            onChange={e => setNumBatches(Number(e.target.value))}
            disabled={running} style={{ ...inputStyle, display: 'block', marginTop: 4 }} />
        </label>
        <label style={labelStyle}>
          Opponent
          <select value={opponent} onChange={e => setOpponent(e.target.value)}
            disabled={running}
            style={{ ...inputStyle, width: '100%', display: 'block', marginTop: 4 }}>
            <option value="self">Self-play</option>
            <option value="heuristic">Heuristic</option>
          </select>
        </label>
        <label style={labelStyle}>
          Device
          <select value={device} onChange={e => setDevice(e.target.value)}
            disabled={running}
            style={{ ...inputStyle, width: '100%', display: 'block', marginTop: 4 }}>
            <option value="cpu">CPU</option>
            <option value="mps">MPS (Apple)</option>
            <option value="cuda">CUDA</option>
          </select>
        </label>
        <label style={labelStyle}>
          Learning rate
          <input type="number" step={0.0001} min={0.00001} max={0.01} value={lr}
            onChange={e => setLr(Number(e.target.value))}
            disabled={running} style={{ ...inputStyle, display: 'block', marginTop: 4 }} />
        </label>
        <label style={labelStyle}>
          Entropy coef
          <input type="number" step={0.001} min={0} max={0.1} value={entropyCoef}
            onChange={e => setEntropyCoef(Number(e.target.value))}
            disabled={running} style={{ ...inputStyle, display: 'block', marginTop: 4 }} />
        </label>
      </div>

      {/* Controls */}
      <div style={{ display: 'flex', gap: 12, alignItems: 'center', marginBottom: 16 }}>
        {!running ? (
          <button onClick={start} style={{
            padding: '8px 24px', borderRadius: 6, border: 'none',
            backgroundColor: '#b45309', color: '#fff', fontSize: 14, fontWeight: 600,
            cursor: 'pointer',
          }}>
            Start PPO Training
          </button>
        ) : (
          <button onClick={stop} style={{
            padding: '8px 24px', borderRadius: 6, border: 'none',
            backgroundColor: '#ef4444', color: '#fff', fontSize: 14, fontWeight: 600,
            cursor: 'pointer',
          }}>
            Stop
          </button>
        )}
        {running && lastBatch && (
          <span style={{ fontSize: 13, color: '#6b7280' }}>
            Batch {lastBatch.batch}/{numBatches} | WR {(lastBatch.win_rate * 100).toFixed(1)}%
          </span>
        )}
        {info && <span style={{ fontSize: 12, color: '#6b7280' }}>{info}</span>}
        {error && <span style={{ fontSize: 12, color: '#ef4444' }}>{error}</span>}
      </div>

      {/* Progress bar */}
      {running && lastBatch && (
        <div style={{ marginBottom: 16 }}>
          <div style={{ height: 6, backgroundColor: '#e5e7eb', borderRadius: 3, overflow: 'hidden' }}>
            <div style={{
              width: `${(lastBatch.batch / numBatches) * 100}%`,
              height: '100%', backgroundColor: '#b45309', borderRadius: 3,
              transition: 'width 0.3s',
            }} />
          </div>
        </div>
      )}

      {/* Results chart (simple text-based) */}
      {batches.length > 0 && (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16 }}>
          {/* Win rate */}
          <div style={{ padding: 12, backgroundColor: '#fff', borderRadius: 8, border: '1px solid #e5e7eb' }}>
            <h4 style={{ fontSize: 13, fontWeight: 600, marginBottom: 8 }}>Win Rate</h4>
            <div style={{ height: 120, display: 'flex', alignItems: 'flex-end', gap: 1 }}>
              {batches.slice(-100).map((b, i) => (
                <div key={i} style={{
                  flex: 1, minWidth: 2,
                  height: `${b.win_rate * 100}%`,
                  backgroundColor: b.win_rate > 0.5 ? '#22c55e' : b.win_rate > 0.4 ? '#eab308' : '#ef4444',
                  borderRadius: '2px 2px 0 0',
                }} />
              ))}
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 11, color: '#9ca3af', marginTop: 4 }}>
              <span>0%</span>
              <span>50%</span>
              <span>100%</span>
            </div>
          </div>

          {/* Losses */}
          <div style={{ padding: 12, backgroundColor: '#fff', borderRadius: 8, border: '1px solid #e5e7eb' }}>
            <h4 style={{ fontSize: 13, fontWeight: 600, marginBottom: 8 }}>Policy Loss</h4>
            <div style={{ height: 120, display: 'flex', alignItems: 'flex-end', gap: 1 }}>
              {(() => {
                const vals = batches.slice(-100).map(b => Math.abs(b.pg_loss));
                const max = Math.max(...vals, 0.001);
                return vals.map((v, i) => (
                  <div key={i} style={{
                    flex: 1, minWidth: 2,
                    height: `${(v / max) * 100}%`,
                    backgroundColor: '#6366f1',
                    borderRadius: '2px 2px 0 0',
                  }} />
                ));
              })()}
            </div>
          </div>

          {/* Stats table */}
          <div style={{
            gridColumn: '1 / -1', padding: 12, backgroundColor: '#fff',
            borderRadius: 8, border: '1px solid #e5e7eb',
          }}>
            <h4 style={{ fontSize: 13, fontWeight: 600, marginBottom: 8 }}>Recent Batches</h4>
            <div style={{ maxHeight: 200, overflowY: 'auto', fontSize: 12 }}>
              <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                <thead>
                  <tr style={{ borderBottom: '1px solid #e5e7eb', color: '#6b7280' }}>
                    <th style={{ textAlign: 'left', padding: 4 }}>Batch</th>
                    <th style={{ textAlign: 'right', padding: 4 }}>WR</th>
                    <th style={{ textAlign: 'right', padding: 4 }}>Steps</th>
                    <th style={{ textAlign: 'right', padding: 4 }}>PG Loss</th>
                    <th style={{ textAlign: 'right', padding: 4 }}>V Loss</th>
                    <th style={{ textAlign: 'right', padding: 4 }}>Entropy</th>
                    <th style={{ textAlign: 'right', padding: 4 }}>Time</th>
                  </tr>
                </thead>
                <tbody>
                  {batches.slice(-20).reverse().map((b) => (
                    <tr key={b.batch} style={{ borderBottom: '1px solid #f3f4f6' }}>
                      <td style={{ padding: 4 }}>{b.batch}</td>
                      <td style={{ textAlign: 'right', padding: 4, fontWeight: 600,
                        color: b.win_rate > 0.5 ? '#166534' : b.win_rate > 0.4 ? '#854d0e' : '#991b1b',
                      }}>
                        {(b.win_rate * 100).toFixed(1)}%
                      </td>
                      <td style={{ textAlign: 'right', padding: 4 }}>{b.steps}</td>
                      <td style={{ textAlign: 'right', padding: 4 }}>{b.pg_loss.toFixed(4)}</td>
                      <td style={{ textAlign: 'right', padding: 4 }}>{b.v_loss.toFixed(4)}</td>
                      <td style={{ textAlign: 'right', padding: 4 }}>{b.entropy.toFixed(3)}</td>
                      <td style={{ textAlign: 'right', padding: 4 }}>{b.elapsed.toFixed(1)}s</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
