import { useState } from 'react';
import { type GameEngine, DIFFICULTY_PRESETS } from '../hooks/useGameEngine';
import { type TreeNode, MoveSource, MoveTarget, cardToString, type GameSnapshot } from '../wasm/types';

function moveSourceLabel(source: number, snap: GameSnapshot | null): string {
  if (source === -1) return 'Root';
  if (!snap) return `src${source}`;
  if (source <= MoveSource.Hand4) {
    const card = snap.hand[source];
    return `Hand ${cardToString(card) || '?'}`;
  }
  if (source === MoveSource.StockPile) {
    return `Stock ${cardToString(snap.stockTop[0]) || '?'}`;
  }
  const di = source - MoveSource.DiscardPile0;
  const top = snap.discardPiles[0][di];
  return `D${di + 1} ${cardToString(top) || '?'}`;
}

function moveTargetLabel(target: number, snap: GameSnapshot | null): string {
  if (target === -1) return '';
  if (!snap) return `tgt${target}`;
  if (target <= MoveTarget.BuildingPile3) {
    const top = snap.buildingPiles[target];
    return `Build ${target + 1}` + (top >= 0 ? ` (${cardToString(top)})` : '');
  }
  const di = target - MoveTarget.DiscardPile0;
  return `Discard ${di + 1}`;
}

function moveLabel(source: number, target: number, snap: GameSnapshot | null): string {
  if (source === -1) return 'Root';
  return `${moveSourceLabel(source, snap)} \u2192 ${moveTargetLabel(target, snap)}`;
}

function rewardColor(reward: number): string {
  if (reward > 0.5) return '#16a34a';
  if (reward > 0) return '#65a30d';
  if (reward > -0.5) return '#ca8a04';
  return '#dc2626';
}

function TreeNodeRow({ node, snapshot, depth, parentVisits }: {
  node: TreeNode;
  snapshot: GameSnapshot | null;
  depth: number;
  parentVisits: number;
}) {
  const [expanded, setExpanded] = useState(depth < 1);
  const hasChildren = node.children.length > 0;
  const visitPct = parentVisits > 0 ? (node.visits / parentVisits * 100) : 100;
  const isRoot = node.parentIndex === -1;

  return (
    <div>
      <div
        onClick={hasChildren ? () => setExpanded(!expanded) : undefined}
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: 8,
          padding: '4px 8px',
          paddingLeft: depth * 24 + 8,
          cursor: hasChildren ? 'pointer' : 'default',
          backgroundColor: depth % 2 === 0 ? '#faf5ff' : '#f5f3ff',
          borderBottom: '1px solid #f3e8ff',
          fontSize: '13px',
          fontFamily: 'monospace',
        }}
      >
        {/* Expand/collapse indicator */}
        <span style={{ width: 16, color: '#9ca3af', fontSize: '11px', flexShrink: 0 }}>
          {hasChildren ? (expanded ? '\u25BC' : '\u25B6') : '\u00B7'}
        </span>

        {/* Player indicator + Move label */}
        {!isRoot && (
          <span style={{
            fontSize: '10px', fontWeight: 600, flexShrink: 0,
            padding: '1px 5px', borderRadius: 3,
            backgroundColor: node.actingPlayer === 0 ? '#dbeafe' : '#fee2e2',
            color: node.actingPlayer === 0 ? '#1e40af' : '#991b1b',
          }}>
            {node.actingPlayer === 0 ? 'You' : 'AI'}
          </span>
        )}
        <span style={{
          fontWeight: isRoot ? 700 : depth === 1 ? 600 : 400,
          color: isRoot ? '#7c3aed' : '#1f2937',
          minWidth: 140,
          flexShrink: 0,
        }}>
          {moveLabel(node.source, node.target, snapshot)}
        </span>

        {/* Visit count + bar */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 6, flex: 1, minWidth: 0 }}>
          <span style={{ color: '#6b7280', fontSize: '12px', width: 60, textAlign: 'right', flexShrink: 0 }}>
            {node.visits.toLocaleString()}
          </span>
          {!isRoot && (
            <div style={{
              flex: 1, height: 10, backgroundColor: '#e5e7eb', borderRadius: 3,
              overflow: 'hidden', maxWidth: 200,
            }}>
              <div style={{
                width: `${Math.min(visitPct, 100)}%`,
                height: '100%',
                backgroundColor: '#8b5cf6',
                borderRadius: 3,
                opacity: 0.4 + (visitPct / 100) * 0.6,
              }} />
            </div>
          )}
          {!isRoot && (
            <span style={{ color: '#9ca3af', fontSize: '11px', width: 40, flexShrink: 0 }}>
              {visitPct.toFixed(1)}%
            </span>
          )}
        </div>

        {/* Avg reward */}
        <span style={{
          color: rewardColor(node.avgReward),
          fontWeight: 600,
          fontSize: '12px',
          width: 60,
          textAlign: 'right',
          flexShrink: 0,
        }}>
          {node.avgReward >= 0 ? '+' : ''}{node.avgReward.toFixed(2)}
        </span>
      </div>

      {/* Children */}
      {expanded && hasChildren && (
        <div>
          {node.children.map((child, i) => (
            <TreeNodeRow
              key={i}
              node={child}
              snapshot={snapshot}
              depth={depth + 1}
              parentVisits={node.visits}
            />
          ))}
        </div>
      )}
    </div>
  );
}

export function MCTSTreeView({ engine }: { engine: GameEngine }) {
  const { snapshot, treeData, isAnalyzingTree, analyzeTree, mctsConfig, setMctsConfig,
    isAIThinking, isLoading } = engine;

  const isMyTurn = snapshot && snapshot.currentPlayer === 0 && !snapshot.isGameOver && !isAIThinking;
  const canAnalyze = isMyTurn && !isAnalyzingTree && !isLoading;

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
          <h2 style={{ margin: 0, fontSize: '20px', fontWeight: 700 }}>MCTS Search Tree</h2>
          <p style={{ margin: '4px 0 0', fontSize: '13px', color: '#6b7280' }}>
            Single determinization — shows how MCTS explored moves
          </p>
        </div>
        <button
          onClick={analyzeTree}
          disabled={!canAnalyze}
          style={{
            padding: '8px 20px', borderRadius: 6, cursor: canAnalyze ? 'pointer' : 'default',
            border: '1px solid #c084fc',
            backgroundColor: isAnalyzingTree ? '#f3e8ff' : canAnalyze ? '#7c3aed' : '#e5e7eb',
            color: isAnalyzingTree ? '#9ca3af' : canAnalyze ? '#fff' : '#9ca3af',
            fontSize: '14px', fontWeight: 600,
          }}
        >
          {isAnalyzingTree ? 'Analyzing...' : 'Analyze Tree'}
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
          Iterations
          <input
            type="number" min={10} max={50000} step={100}
            value={mctsConfig.iterations}
            onChange={e => setMctsConfig({ ...mctsConfig, iterations: Number(e.target.value) })}
            style={{ width: 70, padding: '2px 4px', borderRadius: 4, border: '1px solid #d1d5db', fontSize: '12px' }}
          />
        </label>
        <label style={{ display: 'flex', alignItems: 'center', gap: 4, color: '#6b7280' }}>
          Heuristic %
          <input
            type="number" min={0} max={100} step={10}
            value={mctsConfig.heuristicPct}
            onChange={e => setMctsConfig({ ...mctsConfig, heuristicPct: Number(e.target.value) })}
            style={{ width: 50, padding: '2px 4px', borderRadius: 4, border: '1px solid #d1d5db', fontSize: '12px' }}
          />
        </label>
        <label style={{ display: 'flex', alignItems: 'center', gap: 4, color: '#6b7280' }}>
          Tree depth
          <input
            type="number" min={1} max={20}
            value={mctsConfig.treeDepth}
            onChange={e => setMctsConfig({ ...mctsConfig, treeDepth: Number(e.target.value) })}
            style={{ width: 50, padding: '2px 4px', borderRadius: 4, border: '1px solid #d1d5db', fontSize: '12px' }}
          />
        </label>
        <label style={{ display: 'flex', alignItems: 'center', gap: 4, color: '#6b7280' }}>
          Rollout depth
          <input
            type="number" min={1} max={50}
            value={mctsConfig.rolloutDepth}
            onChange={e => setMctsConfig({ ...mctsConfig, rolloutDepth: Number(e.target.value) })}
            style={{ width: 50, padding: '2px 4px', borderRadius: 4, border: '1px solid #d1d5db', fontSize: '12px' }}
          />
        </label>
        <span style={{ color: '#9ca3af', marginLeft: 'auto' }}>
          {mctsConfig.iterations.toLocaleString()} sims (1 det)
        </span>
      </div>

      {/* Legend */}
      <div style={{
        display: 'flex', gap: 24, marginBottom: 12, fontSize: '11px', color: '#6b7280',
        padding: '0 8px',
      }}>
        <span>Purple bar = visit share of parent</span>
        <span>Right column = avg reward (net stock progress)</span>
      </div>

      {/* Status messages */}
      {!snapshot || snapshot.isGameOver ? (
        <div style={{ padding: 24, textAlign: 'center', color: '#9ca3af' }}>
          {snapshot?.isGameOver ? 'Game is over — start a new game to analyze' : 'Loading...'}
        </div>
      ) : isAIThinking ? (
        <div style={{ padding: 24, textAlign: 'center', color: '#9ca3af' }}>
          AI is thinking...
        </div>
      ) : snapshot.currentPlayer !== 0 ? (
        <div style={{ padding: 24, textAlign: 'center', color: '#9ca3af' }}>
          Waiting for your turn...
        </div>
      ) : !treeData ? (
        <div style={{ padding: 24, textAlign: 'center', color: '#9ca3af' }}>
          {isAnalyzingTree ? 'Building search tree...' : 'Click "Analyze Tree" to explore the MCTS search tree'}
        </div>
      ) : (
        /* Tree */
        <div style={{
          border: '1px solid #e9d5ff', borderRadius: 8, overflow: 'hidden',
        }}>
          {/* Column headers */}
          <div style={{
            display: 'flex', alignItems: 'center', gap: 8,
            padding: '6px 8px', paddingLeft: 32,
            backgroundColor: '#ede9fe', fontSize: '11px', fontWeight: 600,
            color: '#6b7280', borderBottom: '1px solid #e9d5ff',
          }}>
            <span style={{ minWidth: 160 }}>Move</span>
            <div style={{ display: 'flex', alignItems: 'center', gap: 6, flex: 1 }}>
              <span style={{ width: 60, textAlign: 'right' }}>Visits</span>
              <span style={{ flex: 1, maxWidth: 200 }}>Distribution</span>
              <span style={{ width: 40 }}>%</span>
            </div>
            <span style={{ width: 60, textAlign: 'right' }}>Reward</span>
          </div>

          <TreeNodeRow
            node={treeData}
            snapshot={snapshot}
            depth={0}
            parentVisits={treeData.visits}
          />
        </div>
      )}
    </div>
  );
}
