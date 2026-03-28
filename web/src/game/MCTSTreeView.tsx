import { useState, useEffect, useMemo } from 'react';
import { type GameEngine } from '../hooks/useGameEngine';
import {
  MoveTarget, cardToString, isDiscard as isDiscardTarget,
  type GameSnapshot, type MoveTreeNode, type ChainAnalysis,
} from '../wasm/types';

// ---- Mini board ----

function MiniCard({ card, size = 24 }: { card: number; size?: number }) {
  const label = cardToString(card);
  if (!label) return <span style={{ width: size, height: size, display: 'inline-block' }} />;
  const bg = card === 0 ? '#fbbf24' : card <= 4 ? '#3b82f6' : card <= 8 ? '#22c55e' : '#ef4444';
  return (
    <span style={{
      display: 'inline-flex', alignItems: 'center', justifyContent: 'center',
      width: size, height: size, borderRadius: 3, fontSize: size * 0.42,
      fontWeight: 700, color: '#fff', backgroundColor: bg, border: '1px solid rgba(0,0,0,0.15)',
    }}>
      {label}
    </span>
  );
}

function MiniBoard({ snap }: { snap: GameSnapshot }) {
  return (
    <div style={{
      display: 'flex', gap: 12, padding: '6px 10px', backgroundColor: '#f9fafb',
      borderRadius: 6, fontSize: '10px', alignItems: 'center', flexWrap: 'wrap',
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
        <span style={{ color: '#6b7280', fontWeight: 600 }}>Stock</span>
        <MiniCard card={snap.stockTop[0]} />
        <span style={{ color: '#9ca3af' }}>({snap.stockSize[0]})</span>
      </div>
      <div style={{ display: 'flex', alignItems: 'center', gap: 2 }}>
        <span style={{ color: '#6b7280', fontWeight: 600, marginRight: 2 }}>Hand</span>
        {snap.hand.map((c, i) => <MiniCard key={i} card={c} size={22} />)}
      </div>
      <div style={{ display: 'flex', alignItems: 'center', gap: 3 }}>
        <span style={{ color: '#6b7280', fontWeight: 600 }}>Build</span>
        {[0, 1, 2, 3].map(b => (
          <span key={b} style={{
            display: 'inline-flex', alignItems: 'center', justifyContent: 'center',
            width: 22, height: 22, borderRadius: 3, fontSize: 10, fontWeight: 600,
            backgroundColor: snap.buildingPiles[b] >= 0 ? '#ecfdf5' : '#f9fafb',
            border: '1px solid #d1d5db', color: '#374151',
          }}>
            {snap.buildingPiles[b] >= 0 ? snap.buildingPiles[b] : '-'}
          </span>
        ))}
      </div>
      <div style={{ display: 'flex', alignItems: 'center', gap: 3 }}>
        <span style={{ color: '#6b7280', fontWeight: 600 }}>Disc</span>
        {[0, 1, 2, 3].map(d => (
          snap.discardPiles[0][d] >= 0 ? <MiniCard key={d} card={snap.discardPiles[0][d]} size={22} /> :
          <span key={d} style={{
            display: 'inline-flex', width: 22, height: 22, borderRadius: 3,
            border: '1px dashed #d1d5db', alignItems: 'center', justifyContent: 'center',
            fontSize: 9, color: '#d1d5db',
          }}>-</span>
        ))}
      </div>
    </div>
  );
}

// ---- Helpers ----

function targetStr(t: number): string {
  if (t <= MoveTarget.BuildingPile3) return `B${t + 1}`;
  return `D${t - MoveTarget.DiscardPile0 + 1}`;
}

function rewardColor(r: number): string {
  if (r > 0.5) return '#16a34a';
  if (r > 0) return '#65a30d';
  if (r > -0.5) return '#ca8a04';
  return '#dc2626';
}

// ---- Score matching ----
// Build a map from every path prefix -> best chain score that passes through it.
// This gives scores to intermediate tree nodes, not just leaves.
function buildScoreMap(chains: ChainAnalysis[] | null): Map<string, { reward: number; visits: number; bestDiscard: string }> {
  const map = new Map<string, { reward: number; visits: number; bestDiscard: string }>();
  if (!chains) return map;

  for (const chain of chains) {
    const buildParts: string[] = [];
    let discardStr = '';
    for (const m of chain.moves) {
      if (isDiscardTarget(m.target)) {
        discardStr = `${cardToString(m.card)}\u2192${targetStr(m.target)}`;
      } else {
        buildParts.push(`${m.card}:${m.target}`);
      }
    }

    // Register this chain's score at every prefix (including the full path)
    for (let prefixLen = 1; prefixLen <= buildParts.length; prefixLen++) {
      const key = buildParts.slice(0, prefixLen).join('/');
      const existing = map.get(key);
      if (!existing || chain.reward > existing.reward) {
        map.set(key, { reward: chain.reward, visits: chain.visits, bestDiscard: discardStr });
      }
    }
  }
  return map;
}

// Build path key for a tree node by walking up parents
function nodePathKey(node: MoveTreeNode, parentMap: Map<number, MoveTreeNode>): string {
  const parts: string[] = [];
  let cur: MoveTreeNode | undefined = node;
  while (cur && cur.parentIndex !== -1) {
    parts.unshift(`${cur.card}:${cur.target}`);
    cur = parentMap.get(cur.index);
  }
  return parts.join('/');
}

function buildParentMap(root: MoveTreeNode): Map<number, MoveTreeNode> {
  const map = new Map<number, MoveTreeNode>();
  const visit = (parent: MoveTreeNode) => {
    for (const child of parent.children) {
      map.set(child.index, parent);
      visit(child);
    }
  };
  visit(root);
  return map;
}

// ---- Layout ----

const NODE_W = 54;
const NODE_H = 38;
const H_GAP = 40;
const V_GAP = 4;

interface LayoutNode {
  node: MoveTreeNode;
  x: number;
  y: number;
  children: LayoutNode[];
  pathKey: string;
  subtreeHeight: number;
}

function layoutTree(
  node: MoveTreeNode,
  parentMap: Map<number, MoveTreeNode>,
  depth: number,
  yOffset: number,
  expandedSet: Set<number>,
): LayoutNode {
  const x = depth * (NODE_W + H_GAP);
  const isRoot = node.parentIndex === -1;

  const buildChildren = node.children.filter(c => c.nodeType === 0 || c.nodeType === 1 || c.nodeType === 3);
  const discardChildren = node.children.filter(c => c.nodeType === 2);

  const expanded = isRoot || expandedSet.has(node.index);
  let layoutChildren: LayoutNode[] = [];
  let yPos = yOffset;

  if (expanded) {
    for (const child of buildChildren) {
      const lc = layoutTree(child, parentMap, depth + 1, yPos, expandedSet);
      layoutChildren.push(lc);
      yPos += lc.subtreeHeight + V_GAP;
    }
    // Discard group as a single leaf
    if (discardChildren.length > 0) {
      const dgNode: MoveTreeNode = {
        index: -(node.index + 1) * 1000,
        parentIndex: node.index,
        source: -1, target: -1,
        card: discardChildren.length,
        nodeType: 20,
        children: [],
      };
      const lc: LayoutNode = {
        node: dgNode,
        x: (depth + 1) * (NODE_W + H_GAP),
        y: yPos + NODE_H / 2,
        children: [],
        pathKey: '',
        subtreeHeight: NODE_H,
      };
      layoutChildren.push(lc);
      yPos += NODE_H + V_GAP;
    }
  }

  const subtreeHeight = layoutChildren.length > 0
    ? layoutChildren.reduce((s, c) => s + c.subtreeHeight + V_GAP, -V_GAP)
    : NODE_H;

  const y = layoutChildren.length > 0
    ? (layoutChildren[0].y + layoutChildren[layoutChildren.length - 1].y) / 2
    : yOffset + NODE_H / 2;

  const pathKey = isRoot ? '' : nodePathKey(node, parentMap);

  return { node, x, y, children: layoutChildren, pathKey, subtreeHeight };
}

function getTreeBounds(ln: LayoutNode): { w: number; h: number } {
  let w = ln.x + NODE_W;
  let h = ln.y + NODE_H / 2;
  for (const c of ln.children) {
    const cb = getTreeBounds(c);
    w = Math.max(w, cb.w);
    h = Math.max(h, cb.h);
  }
  return { w, h };
}

// ---- SVG tree ----

function TreeSVG({ layout, scoreMap, onToggle, expandedSet }: {
  layout: LayoutNode;
  scoreMap: Map<string, { reward: number; visits: number; bestDiscard: string }>;
  onToggle: (index: number) => void;
  expandedSet: Set<number>;
}) {
  const bounds = getTreeBounds(layout);
  const width = bounds.w + 140;
  const height = bounds.h + 20;

  const allNodes: { ln: LayoutNode; parent: LayoutNode | null }[] = [];
  const collect = (ln: LayoutNode, parent: LayoutNode | null) => {
    allNodes.push({ ln, parent });
    for (const c of ln.children) collect(c, ln);
  };
  collect(layout, null);

  return (
    <svg width={width} height={height + 10} style={{ display: 'block', minWidth: width }}>
      <g transform="translate(10, 10)">
        {/* Links */}
        {allNodes.map(({ ln, parent }) => {
          if (!parent) return null;
          const x1 = parent.x + NODE_W;
          const y1 = parent.y;
          const x2 = ln.x;
          const y2 = ln.y;
          const mx = (x1 + x2) / 2;
          return (
            <path
              key={`link-${ln.node.index}`}
              d={`M ${x1} ${y1} C ${mx} ${y1}, ${mx} ${y2}, ${x2} ${y2}`}
              fill="none" stroke="#d1d5db" strokeWidth={1.5}
            />
          );
        })}

        {/* Nodes */}
        {allNodes.map(({ ln }) => {
          const { node } = ln;
          const isRoot = node.parentIndex === -1;
          if (isRoot) return null;

          const isDiscardGroup = node.nodeType === 20;
          const isStock = node.nodeType === 1;
          const isDraw5 = node.nodeType === 3;
          const hasBuildChildren = node.children.some(c => c.nodeType === 0 || c.nodeType === 1 || c.nodeType === 3);
          const isExpanded = expandedSet.has(node.index);
          const canExpand = hasBuildChildren && !isDiscardGroup;

          let bg = '#fff';
          let border = '#d1d5db';
          let fg = '#374151';
          if (isStock) { bg = '#dcfce7'; border = '#86efac'; fg = '#166534'; }
          else if (isDraw5) { bg = '#fef3c7'; border = '#fcd34d'; fg = '#92400e'; }
          else if (isDiscardGroup) { bg = '#f3e8ff'; border = '#c4b5fd'; fg = '#7c3aed'; }

          const score = scoreMap.get(ln.pathKey);
          const isLeaf = ln.children.length === 0 && !canExpand;

          return (
            <g key={`node-${node.index}`}>
              <rect
                x={ln.x} y={ln.y - NODE_H / 2}
                width={NODE_W} height={NODE_H}
                rx={6} fill={bg} stroke={border} strokeWidth={1.5}
                onClick={() => canExpand && onToggle(node.index)}
                style={{ cursor: canExpand ? 'pointer' : 'default' }}
              />
              {isDiscardGroup ? (
                <text x={ln.x + NODE_W / 2} y={ln.y + 1} textAnchor="middle"
                      fontSize={10} fontWeight={600} fill={fg} fontFamily="system-ui"
                      style={{ pointerEvents: 'none' }}>
                  {node.card} disc
                </text>
              ) : (
                <>
                  <text x={ln.x + NODE_W / 2} y={ln.y - 4} textAnchor="middle"
                        fontSize={13} fontWeight={700} fill={fg} fontFamily="monospace"
                        style={{ pointerEvents: 'none' }}>
                    {cardToString(node.card)}
                  </text>
                  <text x={ln.x + NODE_W / 2} y={ln.y + 11} textAnchor="middle"
                        fontSize={9} fill="#9ca3af" fontFamily="system-ui"
                        style={{ pointerEvents: 'none' }}>
                    {'\u2192'}{targetStr(node.target)}
                  </text>
                </>
              )}

              {/* Expand indicator */}
              {canExpand && !isExpanded && (
                <text x={ln.x + NODE_W + 3} y={ln.y + 4} fontSize={14} fill="#9ca3af"
                      onClick={() => onToggle(node.index)}
                      style={{ cursor: 'pointer', fontWeight: 700 }}>+</text>
              )}

              {/* Score on every node that has one */}
              {score && (
                <>
                  <rect x={ln.x + NODE_W + 4} y={ln.y - 14} width={60} height={28} rx={4}
                        fill="#f9fafb" stroke="#e5e7eb" strokeWidth={1} />
                  <text x={ln.x + NODE_W + 34} y={ln.y - 1} textAnchor="middle"
                        fontSize={12} fontWeight={700} fill={rewardColor(score.reward)} fontFamily="monospace">
                    {(score.reward >= 0 ? '+' : '') + score.reward.toFixed(2)}
                  </text>
                  <text x={ln.x + NODE_W + 34} y={ln.y + 11} textAnchor="middle"
                        fontSize={8} fill="#9ca3af" fontFamily="system-ui">
                    {score.visits.toLocaleString()}v
                  </text>
                </>
              )}
            </g>
          );
        })}
      </g>
    </svg>
  );
}

// ---- Page ----

export function MCTSTreeView({ engine }: { engine: GameEngine }) {
  const { snapshot, moveTree, refreshMoveTree, chains, reanalyze, isAnalyzing,
    mctsConfig, setMctsConfig, isAIThinking } = engine;

  const isMyTurn = snapshot && snapshot.currentPlayer === 0 && !snapshot.isGameOver && !isAIThinking;
  const [expandedSet, setExpandedSet] = useState<Set<number>>(new Set());

  useEffect(() => {
    if (isMyTurn && !moveTree) {
      refreshMoveTree();
      setExpandedSet(new Set());
    }
  }, [isMyTurn, moveTree, refreshMoveTree]);

  const parentMap = useMemo(() => moveTree ? buildParentMap(moveTree) : new Map(), [moveTree]);
  const scoreMap = useMemo(() => buildScoreMap(chains), [chains]);

  const layout = useMemo(() => {
    if (!moveTree) return null;
    return layoutTree(moveTree, parentMap, 0, 0, expandedSet);
  }, [moveTree, parentMap, expandedSet]);

  const handleToggle = (index: number) => {
    setExpandedSet(prev => {
      const next = new Set(prev);
      if (next.has(index)) next.delete(index); else next.add(index);
      return next;
    });
  };

  return (
    <div style={{
      maxWidth: 1400, margin: '0 auto', padding: '16px',
      fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif',
    }}>
      <div style={{
        display: 'flex', justifyContent: 'space-between', alignItems: 'center',
        marginBottom: 8, paddingBottom: 8, borderBottom: '1px solid #e5e7eb',
      }}>
        <div>
          <h2 style={{ margin: 0, fontSize: '18px', fontWeight: 700 }}>Move Tree</h2>
          <p style={{ margin: '2px 0 0', fontSize: '11px', color: '#6b7280' }}>
            Click + to expand branches. Run MCTS to see scores on leaf nodes.
          </p>
        </div>
        <div style={{ display: 'flex', gap: 8 }}>
          <button onClick={refreshMoveTree} disabled={!isMyTurn}
            style={{
              padding: '5px 14px', borderRadius: 6,
              border: '1px solid #d1d5db', backgroundColor: '#fff', color: '#374151',
              fontSize: '12px', fontWeight: 500, cursor: isMyTurn ? 'pointer' : 'default',
            }}>
            Refresh
          </button>
          <button onClick={reanalyze} disabled={!isMyTurn || isAnalyzing}
            style={{
              padding: '5px 14px', borderRadius: 6,
              border: '1px solid #c084fc',
              backgroundColor: isAnalyzing ? '#f3e8ff' : '#7c3aed',
              color: isAnalyzing ? '#9ca3af' : '#fff',
              fontSize: '12px', fontWeight: 600,
              cursor: isMyTurn && !isAnalyzing ? 'pointer' : 'default',
            }}>
            {isAnalyzing ? 'Running...' : 'Run MCTS'}
          </button>
        </div>
      </div>

      {snapshot && !snapshot.isGameOver && <MiniBoard snap={snapshot} />}

      {/* MCTS config */}
      <div style={{
        display: 'flex', gap: 12, margin: '8px 0', fontSize: '11px', color: '#6b7280', alignItems: 'center',
      }}>
        <label style={{ display: 'flex', alignItems: 'center', gap: 3 }}>
          Iters <input type="number" min={10} max={50000} step={100} value={mctsConfig.iterations}
            onChange={e => setMctsConfig({ ...mctsConfig, iterations: Number(e.target.value) })}
            style={{ width: 60, padding: '1px 4px', borderRadius: 3, border: '1px solid #d1d5db', fontSize: '11px' }} />
        </label>
        <label style={{ display: 'flex', alignItems: 'center', gap: 3 }}>
          Dets <input type="number" min={1} max={100} value={mctsConfig.determinizations}
            onChange={e => setMctsConfig({ ...mctsConfig, determinizations: Number(e.target.value) })}
            style={{ width: 40, padding: '1px 4px', borderRadius: 3, border: '1px solid #d1d5db', fontSize: '11px' }} />
        </label>
        <label style={{ display: 'flex', alignItems: 'center', gap: 3 }}>
          Depth <input type="number" min={1} max={10} value={mctsConfig.turnDepth}
            onChange={e => setMctsConfig({ ...mctsConfig, turnDepth: Number(e.target.value) })}
            style={{ width: 35, padding: '1px 4px', borderRadius: 3, border: '1px solid #d1d5db', fontSize: '11px' }} />
        </label>
        <span style={{ color: '#9ca3af' }}>
          {(mctsConfig.iterations * mctsConfig.determinizations).toLocaleString()} sims
        </span>

        {/* Legend */}
        <span style={{ marginLeft: 'auto', display: 'flex', gap: 6 }}>
          <span style={{ padding: '1px 6px', borderRadius: 3, backgroundColor: '#dcfce7', color: '#166534', fontWeight: 600, fontSize: 9 }}>STOCK</span>
          <span style={{ padding: '1px 6px', borderRadius: 3, backgroundColor: '#fef3c7', color: '#92400e', fontWeight: 600, fontSize: 9 }}>DRAW 5</span>
          <span style={{ padding: '1px 6px', borderRadius: 3, backgroundColor: '#f3e8ff', color: '#7c3aed', fontWeight: 600, fontSize: 9 }}>DISCARD</span>
        </span>
      </div>

      {/* Tree */}
      {!snapshot || snapshot.isGameOver ? (
        <div style={{ padding: 40, textAlign: 'center', color: '#9ca3af' }}>
          {snapshot?.isGameOver ? 'Game is over' : 'Loading...'}
        </div>
      ) : !isMyTurn ? (
        <div style={{ padding: 40, textAlign: 'center', color: '#9ca3af' }}>
          {isAIThinking ? 'AI is thinking...' : 'Waiting for your turn...'}
        </div>
      ) : !layout ? (
        <div style={{ padding: 40, textAlign: 'center', color: '#9ca3af' }}>Generating...</div>
      ) : (
        <div style={{ overflowX: 'auto', border: '1px solid #e5e7eb', borderRadius: 8, backgroundColor: '#fff', padding: 8 }}>
          <TreeSVG layout={layout} scoreMap={scoreMap} onToggle={handleToggle} expandedSet={expandedSet} />
        </div>
      )}
    </div>
  );
}
