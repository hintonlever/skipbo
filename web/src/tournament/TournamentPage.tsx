import { useState, useCallback, useRef, useEffect } from 'react';
import type { MatchJob } from './matchWorker';

// AI type codes matching C++ enum: 0=random, 1=heuristic, 2=mcts, 3=heuristic_random_discard
interface AISpec {
  name: string;
  type: number;
  iters: number;
  dets: number;
  turnDepth: number;
}

const DEFAULT_PARTICIPANTS: AISpec[] = [
  { name: 'Random',      type: 0, iters: 0,     dets: 0,  turnDepth: 0 },
  { name: 'Heuristic',   type: 1, iters: 0,     dets: 0,  turnDepth: 0 },
  { name: 'Heur+RandDiscard', type: 3, iters: 0, dets: 0, turnDepth: 0 },
  { name: 'MCTS Easy',   type: 2, iters: 50,    dets: 3,  turnDepth: 2 },
  { name: 'MCTS Medium', type: 2, iters: 300,   dets: 10, turnDepth: 3 },
  { name: 'MCTS Hard',   type: 2, iters: 2000,  dets: 20, turnDepth: 4 },
];

interface PairResult {
  p0Wins: number;
  p1Wins: number;
  played: number;
}

interface EloRating {
  rating: number;
  games: number;
}

function eloExpected(a: number, b: number): number {
  return 1 / (1 + Math.pow(10, (b - a) / 400));
}

function eloUpdate(ratings: EloRating[], winner: number, loser: number) {
  const k = (r: EloRating) => r.games < 30 ? 40 : 20;
  const exp = eloExpected(ratings[winner].rating, ratings[loser].rating);
  ratings[winner].rating += k(ratings[winner]) * (1 - exp);
  ratings[loser].rating += k(ratings[loser]) * (0 - (1 - exp));
  ratings[winner].games++;
  ratings[loser].games++;
}

const NUM_WORKERS = Math.min(navigator.hardwareConcurrency || 4, 8);

function chipBg(type: number): string {
  return type === 0 ? '#fee2e2' : type === 1 ? '#dbeafe' : type === 3 ? '#fef3c7' : '#dcfce7';
}
function chipColor(type: number): string {
  return type === 0 ? '#991b1b' : type === 1 ? '#1e40af' : type === 3 ? '#92400e' : '#166534';
}

export function TournamentPage() {
  const [participants, setParticipants] = useState<AISpec[]>(() => DEFAULT_PARTICIPANTS.map(p => ({ ...p })));
  const [matchesPerPairing, setMatchesPerPairing] = useState(20);
  const [selected, setSelected] = useState<boolean[]>(() => DEFAULT_PARTICIPANTS.map((_, i) => i <= 2));
  const [isRunning, setIsRunning] = useState(false);
  const [progress, setProgress] = useState('');
  const [completed, setCompleted] = useState(0);
  const [totalMatches, setTotalMatches] = useState(0);
  const [results, setResults] = useState<PairResult[][] | null>(null);
  const [elos, setElos] = useState<EloRating[] | null>(null);
  const workersRef = useRef<Worker[]>([]);
  const cancelRef = useRef(false);

  const updateParticipant = (idx: number, field: keyof AISpec, value: number) => {
    setParticipants(prev => prev.map((p, i) => i === idx ? { ...p, [field]: value } : p));
  };

  // Selected indices
  const selectedIndices = selected.map((s, i) => s ? i : -1).filter(i => i >= 0);
  const selectedAIs = selectedIndices.map(i => participants[i]);

  const toggleSelected = (idx: number) => {
    setSelected(prev => prev.map((s, i) => i === idx ? !s : s));
  };

  // Cleanup workers on unmount
  useEffect(() => {
    return () => {
      workersRef.current.forEach(w => w.terminate());
      workersRef.current = [];
    };
  }, []);

  const runTournament = useCallback(() => {
    const indices = selected.map((s, i) => s ? i : -1).filter(i => i >= 0);
    if (indices.length < 2) return;

    setIsRunning(true);
    cancelRef.current = false;

    const n = participants.length;
    const pairResults: PairResult[][] = Array.from({ length: n }, () =>
      Array.from({ length: n }, () => ({ p0Wins: 0, p1Wins: 0, played: 0 }))
    );
    const ratings: EloRating[] = participants.map(() => ({ rating: 1500, games: 0 }));

    // Build all jobs
    const pairings: [number, number][] = [];
    for (let ii = 0; ii < indices.length; ii++) {
      for (let jj = ii + 1; jj < indices.length; jj++) {
        pairings.push([indices[ii], indices[jj]]);
      }
    }

    const jobs: (MatchJob & { aiI: number; aiJ: number; p0Idx: number; p1Idx: number })[] = [];
    const baseSeed = Math.floor(Math.random() * 1000000);
    let jobIndex = 0;

    for (const [i, j] of pairings) {
      const a = participants[i];
      const b = participants[j];
      for (let m = 0; m < matchesPerPairing; m++) {
        const p0 = m % 2 === 0 ? a : b;
        const p1 = m % 2 === 0 ? b : a;
        const p0Idx = m % 2 === 0 ? i : j;
        const p1Idx = m % 2 === 0 ? j : i;
        jobs.push({
          jobIndex,
          p0Type: p0.type, p0Iters: p0.iters, p0Dets: p0.dets, p0Heuristic: 0, p0Rollout: 0, p0Tree: p0.turnDepth,
          p1Type: p1.type, p1Iters: p1.iters, p1Dets: p1.dets, p1Heuristic: 0, p1Rollout: 0, p1Tree: p1.turnDepth,
          seed: baseSeed + jobIndex,
          aiI: i, aiJ: j, p0Idx, p1Idx,
        });
        jobIndex++;
      }
    }

    const total = jobs.length;
    setTotalMatches(total);
    setCompleted(0);
    setResults(null);
    setElos(null);

    // Partition jobs across workers (round-robin)
    const workerJobs: typeof jobs[] = Array.from({ length: NUM_WORKERS }, () => []);
    jobs.forEach((job, idx) => workerJobs[idx % NUM_WORKERS].push(job));

    // Map jobIndex -> metadata for ELO/results updates
    const jobMeta = new Map(jobs.map(j => [j.jobIndex, { aiI: j.aiI, aiJ: j.aiJ, p0Idx: j.p0Idx, p1Idx: j.p1Idx }]));

    let doneCount = 0;
    let updateQueued = false;

    function flushUpdate() {
      updateQueued = false;
      setCompleted(doneCount);
      setResults(pairResults.map(row => row.map(r => ({ ...r }))));
      setElos(ratings.map(r => ({ ...r })));
      setProgress(`${doneCount} / ${total} matches`);

      if (doneCount >= total || cancelRef.current) {
        setIsRunning(false);
        setProgress(cancelRef.current ? 'Cancelled' : `Done! ${doneCount} matches completed.`);
        workersRef.current.forEach(w => w.terminate());
        workersRef.current = [];
      }
    }

    // Spawn workers
    const workers: Worker[] = [];
    for (let w = 0; w < NUM_WORKERS; w++) {
      if (workerJobs[w].length === 0) continue;

      const worker = new Worker(
        new URL('./matchWorker.ts', import.meta.url),
        { type: 'module' }
      );

      worker.onmessage = (e: MessageEvent) => {
        if (cancelRef.current) return;
        if (e.data.type === 'error') {
          console.error('Worker error:', e.data.message);
          setProgress(`Error: ${e.data.message}`);
          setIsRunning(false);
          workersRef.current.forEach(w => w.terminate());
          workersRef.current = [];
          return;
        }
        if (e.data.type === 'result') {
          const { jobIndex: ji, winner } = e.data;
          const meta = jobMeta.get(ji);
          if (!meta) return;

          const { aiI, aiJ, p0Idx, p1Idx } = meta;
          const winnerIdx = winner === 0 ? p0Idx : p1Idx;
          const loserIdx = winner === 0 ? p1Idx : p0Idx;

          if (winnerIdx === aiI) {
            pairResults[aiI][aiJ].p0Wins++;
          } else {
            pairResults[aiI][aiJ].p1Wins++;
          }
          pairResults[aiI][aiJ].played++;
          eloUpdate(ratings, winnerIdx, loserIdx);

          doneCount++;
          if (!updateQueued) {
            updateQueued = true;
            setTimeout(flushUpdate, 100);
          }
        }
      };

      worker.onerror = (e) => {
        console.error('Worker crashed:', e);
        setProgress(`Worker error: ${e.message}`);
        setIsRunning(false);
        workersRef.current.forEach(w => w.terminate());
        workersRef.current = [];
      };

      worker.postMessage({ type: 'run', jobs: workerJobs[w].map(({ aiI, aiJ, p0Idx, p1Idx, ...job }) => job) });
      workers.push(worker);
    }

    workersRef.current = workers;
  }, [matchesPerPairing, selected, participants]);

  const cancel = useCallback(() => {
    cancelRef.current = true;
    workersRef.current.forEach(w => w.terminate());
    workersRef.current = [];
    setIsRunning(false);
    setProgress('Cancelled');
  }, []);

  return (
    <div style={{
      padding: 32, maxWidth: 900, margin: '0 auto',
      fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif',
    }}>
      <h1 style={{ margin: '0 0 24px', fontSize: 24, fontWeight: 700 }}>AI Tournament</h1>

      {/* Participant selection */}
      <div style={{ marginBottom: 20 }}>
        <h2 style={{ fontSize: 14, fontWeight: 600, marginBottom: 8, color: '#374151' }}>Select Participants</h2>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
          {participants.map((ai, idx) => {
            const active = selected[idx];
            const isMCTS = ai.type === 2;
            const inputStyle = {
              width: 60, padding: '2px 4px', borderRadius: 4,
              border: '1px solid #d1d5db', fontSize: '12px',
            };
            const labelStyle = { color: '#6b7280', fontSize: '11px' as const };
            return (
              <div key={idx} style={{
                display: 'flex', alignItems: 'center', gap: 10,
                padding: '6px 12px', borderRadius: 8,
                border: active ? `2px solid ${chipColor(ai.type)}` : '2px solid #e5e7eb',
                backgroundColor: active ? chipBg(ai.type) : '#fafafa',
                opacity: isRunning ? 0.7 : 1,
              }}>
                <button
                  onClick={() => !isRunning && toggleSelected(idx)}
                  disabled={isRunning}
                  style={{
                    padding: '4px 12px', borderRadius: 16, fontSize: 13, fontWeight: 600,
                    cursor: isRunning ? 'default' : 'pointer',
                    border: 'none',
                    backgroundColor: active ? chipColor(ai.type) : '#d1d5db',
                    color: '#fff', minWidth: 100,
                  }}
                >
                  {ai.name}
                </button>
                {isMCTS && (
                  <div style={{ display: 'flex', gap: 8, alignItems: 'center', flexWrap: 'wrap' }}>
                    <label style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                      <span style={labelStyle}>Iters</span>
                      <input type="number" min={10} max={50000} step={10}
                        value={ai.iters} disabled={isRunning}
                        onChange={e => updateParticipant(idx, 'iters', Number(e.target.value))}
                        style={inputStyle} />
                    </label>
                    <label style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                      <span style={labelStyle}>Dets</span>
                      <input type="number" min={1} max={100}
                        value={ai.dets} disabled={isRunning}
                        onChange={e => updateParticipant(idx, 'dets', Number(e.target.value))}
                        style={inputStyle} />
                    </label>
                    <label style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                      <span style={labelStyle}>Turn Depth</span>
                      <input type="number" min={1} max={10}
                        value={ai.turnDepth} disabled={isRunning}
                        onChange={e => updateParticipant(idx, 'turnDepth', Number(e.target.value))}
                        style={inputStyle} />
                    </label>
                    <span style={{ fontSize: 11, color: '#9ca3af' }}>
                      {(ai.iters * ai.dets).toLocaleString()} sims/move
                    </span>
                  </div>
                )}
              </div>
            );
          })}
        </div>
        {selectedIndices.length < 2 && (
          <p style={{ fontSize: 12, color: '#ef4444', marginTop: 4 }}>Select at least 2 participants.</p>
        )}
      </div>

      {/* Controls */}
      <div style={{
        display: 'flex', gap: 16, alignItems: 'center', marginBottom: 24,
        padding: 16, backgroundColor: '#f9fafb', borderRadius: 8,
      }}>
        <label style={{ display: 'flex', alignItems: 'center', gap: 8, fontSize: 14 }}>
          Matches per pairing:
          <input
            type="number" min={2} max={500} step={2}
            value={matchesPerPairing}
            onChange={e => setMatchesPerPairing(Number(e.target.value))}
            disabled={isRunning}
            style={{ width: 70, padding: '4px 8px', borderRadius: 4, border: '1px solid #d1d5db', fontSize: 14 }}
          />
        </label>
        <span style={{ fontSize: 12, color: '#9ca3af' }}>
          {NUM_WORKERS} workers
        </span>
        {!isRunning ? (
          <button
            onClick={runTournament}
            disabled={selectedIndices.length < 2}
            style={{
              padding: '6px 20px', borderRadius: 6,
              cursor: selectedIndices.length < 2 ? 'default' : 'pointer',
              border: 'none',
              backgroundColor: selectedIndices.length < 2 ? '#d1d5db' : '#6366f1',
              color: '#fff', fontSize: 14, fontWeight: 600,
            }}
          >
            Run Tournament
          </button>
        ) : (
          <button
            onClick={cancel}
            style={{
              padding: '6px 20px', borderRadius: 6, cursor: 'pointer',
              border: 'none', backgroundColor: '#ef4444', color: '#fff', fontSize: 14, fontWeight: 600,
            }}
          >
            Cancel
          </button>
        )}
        {progress && (
          <span style={{ fontSize: 13, color: '#6b7280' }}>{progress}</span>
        )}
      </div>

      {/* Progress bar */}
      {isRunning && totalMatches > 0 && (
        <div style={{ marginBottom: 20 }}>
          <div style={{ height: 6, backgroundColor: '#e5e7eb', borderRadius: 3, overflow: 'hidden' }}>
            <div style={{
              width: `${(completed / totalMatches) * 100}%`,
              height: '100%', backgroundColor: '#6366f1', borderRadius: 3,
              transition: 'width 0.2s',
            }} />
          </div>
        </div>
      )}

      {/* ELO Rankings */}
      {elos && (
        <div style={{ marginBottom: 24 }}>
          <h2 style={{ fontSize: 16, fontWeight: 600, marginBottom: 8 }}>ELO Rankings</h2>
          <table style={{ borderCollapse: 'collapse', width: '100%', fontSize: 14 }}>
            <thead>
              <tr style={{ borderBottom: '2px solid #e5e7eb' }}>
                <th style={{ textAlign: 'left', padding: '8px 12px' }}>Rank</th>
                <th style={{ textAlign: 'left', padding: '8px 12px' }}>AI</th>
                <th style={{ textAlign: 'right', padding: '8px 12px' }}>ELO</th>
                <th style={{ textAlign: 'right', padding: '8px 12px' }}>Games</th>
              </tr>
            </thead>
            <tbody>
              {[...elos]
                .map((e, i) => ({ ...e, name: participants[i].name, idx: i, type: participants[i].type }))
                .filter(e => selectedIndices.includes(e.idx))
                .sort((a, b) => b.rating - a.rating)
                .map((e, rank) => (
                  <tr key={e.idx} style={{ borderBottom: '1px solid #f3f4f6' }}>
                    <td style={{ padding: '6px 12px', color: '#9ca3af' }}>{rank + 1}</td>
                    <td style={{ padding: '6px 12px', fontWeight: 500 }}>
                      <span style={{
                        padding: '2px 8px', borderRadius: 10, fontSize: 12,
                        backgroundColor: chipBg(e.type), color: chipColor(e.type),
                      }}>
                        {e.name}
                      </span>
                    </td>
                    <td style={{ padding: '6px 12px', textAlign: 'right', fontWeight: 700, fontVariantNumeric: 'tabular-nums' }}>
                      {Math.round(e.rating)}
                    </td>
                    <td style={{ padding: '6px 12px', textAlign: 'right', color: '#6b7280' }}>{e.games}</td>
                  </tr>
                ))}
            </tbody>
          </table>
        </div>
      )}

      {/* Win Rate Matrix */}
      {results && (
        <div>
          <h2 style={{ fontSize: 16, fontWeight: 600, marginBottom: 8 }}>Win Rate Matrix</h2>
          <div style={{ overflowX: 'auto' }}>
            <table style={{ borderCollapse: 'collapse', fontSize: 13 }}>
              <thead>
                <tr>
                  <th style={{ padding: '6px 10px', borderBottom: '2px solid #e5e7eb' }}></th>
                  {selectedAIs.map(ai => (
                    <th key={ai.name} style={{
                      padding: '6px 10px', borderBottom: '2px solid #e5e7eb',
                      textAlign: 'center', whiteSpace: 'nowrap', fontWeight: 600,
                    }}>
                      {ai.name}
                    </th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {selectedIndices.map(i => (
                  <tr key={i}>
                    <td style={{
                      padding: '6px 10px', fontWeight: 600, whiteSpace: 'nowrap',
                      borderBottom: '1px solid #f3f4f6',
                    }}>
                      {participants[i].name}
                    </td>
                    {selectedIndices.map(j => {
                      if (i === j) {
                        return (
                          <td key={j} style={{
                            padding: '6px 10px', textAlign: 'center',
                            backgroundColor: '#f3f4f6', borderBottom: '1px solid #f3f4f6', color: '#9ca3af',
                          }}>-</td>
                        );
                      }
                      const mi = Math.min(i, j);
                      const ma = Math.max(i, j);
                      const pair = results[mi][ma];
                      if (!pair || pair.played === 0) {
                        return (
                          <td key={j} style={{
                            padding: '6px 10px', textAlign: 'center',
                            borderBottom: '1px solid #f3f4f6', color: '#d1d5db',
                          }}>...</td>
                        );
                      }
                      const iWins = i === mi ? pair.p0Wins : pair.p1Wins;
                      const winRate = iWins / pair.played;
                      const pct = (winRate * 100).toFixed(0);
                      const bg = winRate > 0.6 ? '#dcfce7' : winRate < 0.4 ? '#fee2e2' : '#fefce8';
                      const color = winRate > 0.6 ? '#166534' : winRate < 0.4 ? '#991b1b' : '#854d0e';
                      return (
                        <td key={j} style={{
                          padding: '6px 10px', textAlign: 'center',
                          backgroundColor: bg, color, fontWeight: 600,
                          borderBottom: '1px solid #f3f4f6', fontVariantNumeric: 'tabular-nums',
                        }}>
                          {pct}%
                          <span style={{ fontSize: 11, fontWeight: 400, color: '#9ca3af', marginLeft: 4 }}>
                            ({iWins}/{pair.played})
                          </span>
                        </td>
                      );
                    })}
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          <p style={{ fontSize: 12, color: '#9ca3af', marginTop: 8 }}>
            Cell shows row player's win rate against column player.
          </p>
        </div>
      )}
    </div>
  );
}
