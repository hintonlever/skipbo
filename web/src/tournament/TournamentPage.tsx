import { useState, useCallback, useRef } from 'react';
import { initEngine, vectorToArray, type SkipBoModule } from '../wasm/engine';

// AI type codes matching C++ enum: 0=random, 1=heuristic, 2=mcts
interface AISpec {
  name: string;
  type: number; // 0=random, 1=heuristic, 2=mcts
  iters: number;
  dets: number;
  heuristicPct: number;
  rolloutDepth: number;
  treeDepth: number;
}

const AI_PARTICIPANTS: AISpec[] = [
  { name: 'Random',      type: 0, iters: 0,     dets: 0,  heuristicPct: 0,  rolloutDepth: 0,  treeDepth: 0 },
  { name: 'Heuristic',   type: 1, iters: 0,     dets: 0,  heuristicPct: 0,  rolloutDepth: 0,  treeDepth: 0 },
  { name: 'MCTS Easy',   type: 2, iters: 50,    dets: 3,  heuristicPct: 50, rolloutDepth: 10, treeDepth: 5 },
  { name: 'MCTS Medium', type: 2, iters: 300,   dets: 10, heuristicPct: 50, rolloutDepth: 10, treeDepth: 5 },
  { name: 'MCTS Hard',   type: 2, iters: 10000, dets: 20, heuristicPct: 50, rolloutDepth: 10, treeDepth: 5 },
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

export function TournamentPage() {
  const [matchesPerPairing, setMatchesPerPairing] = useState(20);
  const [isRunning, setIsRunning] = useState(false);
  const [progress, setProgress] = useState('');
  const [results, setResults] = useState<PairResult[][] | null>(null);
  const [elos, setElos] = useState<EloRating[] | null>(null);
  const cancelRef = useRef(false);

  const runTournament = useCallback(async () => {
    setIsRunning(true);
    cancelRef.current = false;

    const module: SkipBoModule = await initEngine();
    const n = AI_PARTICIPANTS.length;
    const pairResults: PairResult[][] = Array.from({ length: n }, () =>
      Array.from({ length: n }, () => ({ p0Wins: 0, p1Wins: 0, played: 0 }))
    );
    const ratings: EloRating[] = AI_PARTICIPANTS.map(() => ({ rating: 1500, games: 0 }));

    // Generate all pairings
    const pairings: [number, number][] = [];
    for (let i = 0; i < n; i++) {
      for (let j = i + 1; j < n; j++) {
        pairings.push([i, j]);
      }
    }

    const totalMatches = pairings.length * matchesPerPairing;
    let completed = 0;
    const baseSeed = Math.floor(Math.random() * 1000000);

    for (const [i, j] of pairings) {
      const a = AI_PARTICIPANTS[i];
      const b = AI_PARTICIPANTS[j];

      for (let m = 0; m < matchesPerPairing; m++) {
        if (cancelRef.current) break;

        // Alternate who goes first
        const p0 = m % 2 === 0 ? a : b;
        const p1 = m % 2 === 0 ? b : a;
        const p0Idx = m % 2 === 0 ? i : j;
        const p1Idx = m % 2 === 0 ? j : i;
        const seed = baseSeed + completed;

        // Run match via WASM
        const result = vectorToArray(module.runMatch(
          p0.type, p0.iters, p0.dets, p0.heuristicPct, p0.rolloutDepth, p0.treeDepth,
          p1.type, p1.iters, p1.dets, p1.heuristicPct, p1.rolloutDepth, p1.treeDepth,
          seed
        ));

        const winner = result[0]; // 0 or 1
        const winnerIdx = winner === 0 ? p0Idx : p1Idx;
        const loserIdx = winner === 0 ? p1Idx : p0Idx;

        // Update pair results (always stored as [i][j] where i < j)
        if (winnerIdx === i) {
          pairResults[i][j].p0Wins++;
        } else {
          pairResults[i][j].p1Wins++;
        }
        pairResults[i][j].played++;

        // Update ELO
        eloUpdate(ratings, winnerIdx, loserIdx);

        completed++;

        // Yield to UI every match
        if (completed % 1 === 0) {
          setProgress(`${completed} / ${totalMatches} matches (${a.name} vs ${b.name})`);
          setResults(pairResults.map(row => row.map(r => ({ ...r }))));
          setElos(ratings.map(r => ({ ...r })));
          await new Promise(r => setTimeout(r, 0));
        }
      }
      if (cancelRef.current) break;
    }

    setResults(pairResults);
    setElos(ratings.map(r => ({ ...r })));
    setProgress(cancelRef.current ? 'Cancelled' : `Done! ${completed} matches completed.`);
    setIsRunning(false);
  }, [matchesPerPairing]);

  const cancel = useCallback(() => {
    cancelRef.current = true;
  }, []);

  return (
    <div style={{
      padding: 32, maxWidth: 900, margin: '0 auto',
      fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif',
    }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 24 }}>
        <h1 style={{ margin: 0, fontSize: 24, fontWeight: 700 }}>AI Tournament</h1>
        <a href="#" style={{ fontSize: 13, color: '#6366f1' }}>Back to Game</a>
      </div>

      {/* Controls */}
      <div style={{
        display: 'flex', gap: 16, alignItems: 'center', marginBottom: 24,
        padding: 16, backgroundColor: '#f9fafb', borderRadius: 8,
      }}>
        <label style={{ display: 'flex', alignItems: 'center', gap: 8, fontSize: 14 }}>
          Matches per pairing:
          <input
            type="number"
            min={2}
            max={500}
            step={2}
            value={matchesPerPairing}
            onChange={e => setMatchesPerPairing(Number(e.target.value))}
            disabled={isRunning}
            style={{
              width: 70, padding: '4px 8px', borderRadius: 4,
              border: '1px solid #d1d5db', fontSize: 14,
            }}
          />
        </label>
        {!isRunning ? (
          <button
            onClick={runTournament}
            style={{
              padding: '6px 20px', borderRadius: 6, cursor: 'pointer',
              border: 'none', backgroundColor: '#6366f1', color: '#fff',
              fontSize: 14, fontWeight: 600,
            }}
          >
            Run Tournament
          </button>
        ) : (
          <button
            onClick={cancel}
            style={{
              padding: '6px 20px', borderRadius: 6, cursor: 'pointer',
              border: 'none', backgroundColor: '#ef4444', color: '#fff',
              fontSize: 14, fontWeight: 600,
            }}
          >
            Cancel
          </button>
        )}
        {progress && (
          <span style={{ fontSize: 13, color: '#6b7280' }}>{progress}</span>
        )}
      </div>

      {/* Participants */}
      <div style={{ marginBottom: 24 }}>
        <h2 style={{ fontSize: 16, fontWeight: 600, marginBottom: 8 }}>Participants</h2>
        <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
          {AI_PARTICIPANTS.map(ai => (
            <span key={ai.name} style={{
              padding: '4px 12px', borderRadius: 16, fontSize: 13,
              backgroundColor: ai.type === 0 ? '#fee2e2' : ai.type === 1 ? '#dbeafe' : '#dcfce7',
              color: ai.type === 0 ? '#991b1b' : ai.type === 1 ? '#1e40af' : '#166534',
              fontWeight: 500,
            }}>
              {ai.name}
            </span>
          ))}
        </div>
      </div>

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
                .map((e, i) => ({ ...e, name: AI_PARTICIPANTS[i].name, idx: i }))
                .sort((a, b) => b.rating - a.rating)
                .map((e, rank) => (
                  <tr key={e.idx} style={{ borderBottom: '1px solid #f3f4f6' }}>
                    <td style={{ padding: '6px 12px', color: '#9ca3af' }}>{rank + 1}</td>
                    <td style={{ padding: '6px 12px', fontWeight: 500 }}>{e.name}</td>
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
                  {AI_PARTICIPANTS.map(ai => (
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
                {AI_PARTICIPANTS.map((rowAi, i) => (
                  <tr key={rowAi.name}>
                    <td style={{
                      padding: '6px 10px', fontWeight: 600, whiteSpace: 'nowrap',
                      borderBottom: '1px solid #f3f4f6',
                    }}>
                      {rowAi.name}
                    </td>
                    {AI_PARTICIPANTS.map((_, j) => {
                      if (i === j) {
                        return (
                          <td key={j} style={{
                            padding: '6px 10px', textAlign: 'center',
                            backgroundColor: '#f3f4f6', borderBottom: '1px solid #f3f4f6',
                            color: '#9ca3af',
                          }}>
                            -
                          </td>
                        );
                      }
                      // results[min][max] stores { p0Wins (for min idx), p1Wins (for max idx) }
                      const mi = Math.min(i, j);
                      const ma = Math.max(i, j);
                      const pair = results[mi][ma];
                      if (!pair || pair.played === 0) {
                        return (
                          <td key={j} style={{
                            padding: '6px 10px', textAlign: 'center',
                            borderBottom: '1px solid #f3f4f6', color: '#d1d5db',
                          }}>
                            ...
                          </td>
                        );
                      }
                      // Win rate of row player (i) against column player (j)
                      const iWins = i === mi ? pair.p0Wins : pair.p1Wins;
                      const winRate = iWins / pair.played;
                      const pct = (winRate * 100).toFixed(0);
                      const bg = winRate > 0.6 ? '#dcfce7' : winRate < 0.4 ? '#fee2e2' : '#fefce8';
                      const color = winRate > 0.6 ? '#166534' : winRate < 0.4 ? '#991b1b' : '#854d0e';
                      return (
                        <td key={j} style={{
                          padding: '6px 10px', textAlign: 'center',
                          backgroundColor: bg, color, fontWeight: 600,
                          borderBottom: '1px solid #f3f4f6',
                          fontVariantNumeric: 'tabular-nums',
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
