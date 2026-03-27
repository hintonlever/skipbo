import { useState } from 'react';
import { type GameEngine, DIFFICULTY_PRESETS } from '../hooks/useGameEngine';
import { MoveSource, MoveTarget, cardToString, isDiscard as isDiscardTarget, type GameSnapshot, type ChainAnalysis } from '../wasm/types';

function sourceLabel(s: number, snap: GameSnapshot | null): string {
  if (!snap) return `src${s}`;
  if (s <= MoveSource.Hand4) {
    const card = snap.hand[s];
    return cardToString(card) || '?';
  }
  if (s === MoveSource.StockPile) {
    return `Stk(${cardToString(snap.stockTop[0]) || '?'})`;
  }
  const di = s - MoveSource.DiscardPile0;
  const top = snap.discardPiles[0][di];
  return `D${di + 1}(${cardToString(top) || '?'})`;
}

function targetLabel(t: number, snap: GameSnapshot | null): string {
  if (!snap) return `tgt${t}`;
  if (t <= MoveTarget.BuildingPile3) {
    const top = snap.buildingPiles[t];
    return `B${t + 1}` + (top >= 0 ? `(${cardToString(top)})` : '');
  }
  const di = t - MoveTarget.DiscardPile0;
  return `D${di + 1}`;
}

function rewardColor(r: number): string {
  if (r > 0.5) return '#16a34a';
  if (r > 0) return '#65a30d';
  if (r > -0.5) return '#ca8a04';
  return '#dc2626';
}

function ChainRow({ chain, snapshot, rank, isBest }: {
  chain: ChainAnalysis;
  snapshot: GameSnapshot | null;
  rank: number;
  isBest: boolean;
}) {
  const [expanded, setExpanded] = useState(isBest);
  const builds = chain.moves.filter(m => !isDiscardTarget(m.target as MoveTarget));
  const disc = chain.moves.find(m => isDiscardTarget(m.target as MoveTarget));
  const rewardStr = (chain.reward >= 0 ? '+' : '') + chain.reward.toFixed(3);

  return (
    <div style={{
      borderBottom: '1px solid #f3e8ff',
      backgroundColor: isBest ? '#faf5ff' : rank % 2 === 0 ? '#fff' : '#fafafa',
    }}>
      {/* Summary row */}
      <div
        onClick={() => setExpanded(!expanded)}
        style={{
          display: 'flex', alignItems: 'center', gap: 8,
          padding: '6px 10px', cursor: 'pointer', fontSize: '13px',
        }}
      >
        <span style={{ width: 20, color: '#9ca3af', fontSize: '11px', flexShrink: 0 }}>
          #{rank + 1}
        </span>
        <span style={{ flex: 1, fontSize: '12px', color: '#4b5563' }}>
          {builds.length > 0 ? (
            builds.map((m, j) => (
              <span key={j}>
                {j > 0 && ' \u2192 '}
                <span style={{ fontWeight: 500 }}>{sourceLabel(m.source as number, snapshot)}</span>
                <span style={{ color: '#9ca3af' }}>\u2192</span>
                {targetLabel(m.target as number, snapshot)}
              </span>
            ))
          ) : (
            <span style={{ color: '#9ca3af', fontStyle: 'italic' }}>no builds</span>
          )}
          {disc && (
            <span style={{ color: '#7c3aed', marginLeft: 6 }}>
              | discard {sourceLabel(disc.source as number, snapshot)}\u2192{targetLabel(disc.target as number, snapshot)}
            </span>
          )}
        </span>
        <span style={{
          color: rewardColor(chain.reward), fontWeight: 700,
          fontSize: '12px', fontVariantNumeric: 'tabular-nums',
          flexShrink: 0, width: 55, textAlign: 'right',
        }}>
          {rewardStr}
        </span>
        <span style={{ color: '#9ca3af', fontSize: '11px', flexShrink: 0, width: 50, textAlign: 'right' }}>
          {chain.visits.toLocaleString()}
        </span>
      </div>

      {/* Expanded detail */}
      {expanded && (
        <div style={{ padding: '4px 10px 8px 38px', fontSize: '12px' }}>
          {chain.moves.map((m, j) => {
            const isDisc = isDiscardTarget(m.target as MoveTarget);
            return (
              <div key={j} style={{
                display: 'flex', gap: 8, padding: '2px 0',
                color: isDisc ? '#7c3aed' : '#374151',
              }}>
                <span style={{ width: 16, color: '#d1d5db', textAlign: 'right' }}>{j + 1}.</span>
                <span style={{ fontWeight: 500 }}>{sourceLabel(m.source as number, snapshot)}</span>
                <span style={{ color: '#9ca3af' }}>\u2192</span>
                <span>{targetLabel(m.target as number, snapshot)}</span>
                {isDisc && <span style={{ color: '#9ca3af', fontStyle: 'italic', marginLeft: 4 }}>(end turn)</span>}
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
}

export function MCTSTreeView({ engine }: { engine: GameEngine }) {
  const { snapshot, chains, isAnalyzing, reanalyze, mctsConfig, setMctsConfig,
    isAIThinking, isLoading } = engine;

  const isMyTurn = snapshot && snapshot.currentPlayer === 0 && !snapshot.isGameOver && !isAIThinking;
  const canAnalyze = isMyTurn && !isAnalyzing && !isLoading;
  const bestReward = chains && chains.length > 0 ? chains[0].reward : 0;

  return (
    <div style={{
      maxWidth: 900, margin: '0 auto', padding: '16px',
      fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif',
    }}>
      {/* Header */}
      <div style={{
        display: 'flex', justifyContent: 'space-between', alignItems: 'center',
        marginBottom: 16, paddingBottom: 12, borderBottom: '1px solid #e5e7eb',
      }}>
        <div>
          <h2 style={{ margin: 0, fontSize: '20px', fontWeight: 700 }}>MCTS Chain Analysis</h2>
          <p style={{ margin: '4px 0 0', fontSize: '13px', color: '#6b7280' }}>
            Complete turn sequences ranked by MCTS evaluation (static eval, no rollouts)
          </p>
        </div>
        <button
          onClick={reanalyze}
          disabled={!canAnalyze}
          style={{
            padding: '8px 20px', borderRadius: 6, cursor: canAnalyze ? 'pointer' : 'default',
            border: '1px solid #c084fc',
            backgroundColor: isAnalyzing ? '#f3e8ff' : canAnalyze ? '#7c3aed' : '#e5e7eb',
            color: isAnalyzing ? '#9ca3af' : canAnalyze ? '#fff' : '#9ca3af',
            fontSize: '14px', fontWeight: 600,
          }}
        >
          {isAnalyzing ? 'Analyzing...' : 'Analyze'}
        </button>
      </div>

      {/* Config */}
      <div style={{
        display: 'flex', gap: 16, marginBottom: 16, padding: '10px 12px',
        backgroundColor: '#f9fafb', borderRadius: 8, fontSize: '12px', alignItems: 'center',
        flexWrap: 'wrap',
      }}>
        {DIFFICULTY_PRESETS.filter(p => p.aiType === 'mcts').map((p) => {
          const active = mctsConfig.iterations === p.config.iterations
            && mctsConfig.determinizations === p.config.determinizations;
          return (
            <button
              key={p.label}
              onClick={() => setMctsConfig(p.config)}
              style={{
                padding: '3px 10px', borderRadius: 4, cursor: 'pointer',
                border: '1px solid #d1d5db',
                backgroundColor: active ? '#dbeafe' : '#fff',
                fontWeight: active ? 600 : 400, fontSize: '12px',
              }}
            >
              {p.label}
            </button>
          );
        })}
        <span style={{ color: '#6b7280' }}>|</span>
        <label style={{ display: 'flex', alignItems: 'center', gap: 4, color: '#6b7280' }}>
          Iters
          <input type="number" min={10} max={50000} step={100}
            value={mctsConfig.iterations}
            onChange={e => setMctsConfig({ ...mctsConfig, iterations: Number(e.target.value) })}
            style={{ width: 70, padding: '2px 4px', borderRadius: 4, border: '1px solid #d1d5db', fontSize: '12px' }}
          />
        </label>
        <label style={{ display: 'flex', alignItems: 'center', gap: 4, color: '#6b7280' }}>
          Dets
          <input type="number" min={1} max={100}
            value={mctsConfig.determinizations}
            onChange={e => setMctsConfig({ ...mctsConfig, determinizations: Number(e.target.value) })}
            style={{ width: 50, padding: '2px 4px', borderRadius: 4, border: '1px solid #d1d5db', fontSize: '12px' }}
          />
        </label>
        <label style={{ display: 'flex', alignItems: 'center', gap: 4, color: '#6b7280' }}>
          Turn depth
          <input type="number" min={1} max={10}
            value={mctsConfig.turnDepth}
            onChange={e => setMctsConfig({ ...mctsConfig, turnDepth: Number(e.target.value) })}
            style={{ width: 50, padding: '2px 4px', borderRadius: 4, border: '1px solid #d1d5db', fontSize: '12px' }}
          />
        </label>
        <span style={{ color: '#9ca3af', marginLeft: 'auto' }}>
          {(mctsConfig.iterations * mctsConfig.determinizations).toLocaleString()} total iters
        </span>
      </div>

      {/* Status / results */}
      {!snapshot || snapshot.isGameOver ? (
        <div style={{ padding: 24, textAlign: 'center', color: '#9ca3af' }}>
          {snapshot?.isGameOver ? 'Game is over' : 'Loading...'}
        </div>
      ) : isAIThinking ? (
        <div style={{ padding: 24, textAlign: 'center', color: '#9ca3af' }}>AI is thinking...</div>
      ) : snapshot.currentPlayer !== 0 ? (
        <div style={{ padding: 24, textAlign: 'center', color: '#9ca3af' }}>Waiting for your turn...</div>
      ) : !chains ? (
        <div style={{ padding: 24, textAlign: 'center', color: '#9ca3af' }}>
          {isAnalyzing ? 'Generating chains...' : 'Click "Analyze" to evaluate turn options'}
        </div>
      ) : chains.length === 0 ? (
        <div style={{ padding: 24, textAlign: 'center', color: '#9ca3af' }}>No chains generated</div>
      ) : (
        <div style={{ border: '1px solid #e9d5ff', borderRadius: 8, overflow: 'hidden' }}>
          {/* Header */}
          <div style={{
            display: 'flex', alignItems: 'center', gap: 8,
            padding: '6px 10px', backgroundColor: '#ede9fe',
            fontSize: '11px', fontWeight: 600, color: '#6b7280',
            borderBottom: '1px solid #e9d5ff',
          }}>
            <span style={{ width: 20 }}>#</span>
            <span style={{ flex: 1 }}>Build chain + discard</span>
            <span style={{ width: 55, textAlign: 'right' }}>Reward</span>
            <span style={{ width: 50, textAlign: 'right' }}>Visits</span>
          </div>

          {chains.map((chain, i) => (
            <ChainRow
              key={i}
              chain={chain}
              snapshot={snapshot}
              rank={i}
              isBest={Math.abs(chain.reward - bestReward) < 0.001}
            />
          ))}

          <div style={{ padding: '8px 10px', fontSize: '11px', color: '#9ca3af', backgroundColor: '#f9fafb' }}>
            {chains.length} chains evaluated
          </div>
        </div>
      )}
    </div>
  );
}
