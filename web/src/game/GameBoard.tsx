import { useState, useMemo } from 'react';
import { CardView } from './CardView';
import { PileView } from './PileView';
import { DiscardPileView } from './DiscardPileView';
import { type GameEngine, DIFFICULTY_PRESETS, type AIType } from '../hooks/useGameEngine';
import { MoveSource, MoveTarget, cardToString, type GameSnapshot } from '../wasm/types';

function snapshotToText(snap: GameSnapshot): string {
  const c = (card: number) => cardToString(card) || '--';
  const pileStr = (cards: number[]) =>
    cards.length === 0 ? 'empty' : cards.map(c).join(', ');

  const lines: string[] = ['=== SKIP-BO GAME STATE ==='];
  lines.push(`Turn: ${snap.currentPlayer === 0 ? 'You' : 'AI'}${snap.isGameOver ? ` | Game over — ${snap.winner === 0 ? 'You win' : 'AI wins'}` : ''}`);
  lines.push('');

  // AI
  lines.push(`--- AI ---`);
  lines.push(`Stock: ${c(snap.stockTop[1])} (${snap.stockSize[1]} cards)`);
  for (let d = 0; d < 4; d++) {
    lines.push(`Discard ${d + 1}: ${pileStr(snap.discardPileCards[1][d])}`);
  }
  lines.push('');

  // Building piles
  lines.push('--- Building Piles ---');
  for (let b = 0; b < 4; b++) {
    lines.push(`Pile ${b + 1}: ${c(snap.buildingPiles[b])}${snap.buildingPileSizes[b] > 0 ? ` (${snap.buildingPileSizes[b]} deep)` : ''}`);
  }
  lines.push('');

  // Player
  lines.push('--- You ---');
  lines.push(`Stock: ${c(snap.stockTop[0])} (${snap.stockSize[0]} cards)`);
  for (let d = 0; d < 4; d++) {
    lines.push(`Discard ${d + 1}: ${pileStr(snap.discardPileCards[0][d])}`);
  }
  lines.push(`Hand: ${snap.hand.map(c).join(', ')}`);

  return lines.join('\n');
}

interface GameBoardProps {
  engine: GameEngine;
}

function sourceCard(s: MoveSource, snap: GameSnapshot): string {
  if (s <= MoveSource.Hand4) return cardToString(snap.hand[s]);
  if (s === MoveSource.StockPile) return cardToString(snap.stockTop[0]);
  const di = s - MoveSource.DiscardPile0;
  return cardToString(snap.discardPiles[0][di]);
}

function sourceLabel(s: MoveSource, snap: GameSnapshot): string {
  const card = sourceCard(s, snap);
  if (s <= MoveSource.Hand4) return card;
  if (s === MoveSource.StockPile) return `Stock ${card}`;
  return `D${s - MoveSource.DiscardPile0 + 1} ${card}`;
}

function targetLabel(t: MoveTarget, snap: GameSnapshot): string {
  if (t <= MoveTarget.BuildingPile3) {
    const top = snap.buildingPiles[t];
    return `Build ${t + 1}` + (top >= 0 ? ` (${cardToString(top)})` : '');
  }
  const di = t - MoveTarget.DiscardPile0;
  const top = snap.discardPiles[0][di];
  return `D${di + 1}` + (top >= 0 ? ` (${cardToString(top)})` : '');
}

export function GameBoard({ engine }: GameBoardProps) {
  const { snapshot, isAIThinking, isAnalyzing, playMove, newGame, aiType, mctsConfig, setMctsConfig,
    setAIType, analysis, showAnalysis, toggleAnalysis, reanalyze, isLoading } = engine;
  const [selectedSource, setSelectedSource] = useState<MoveSource | null>(null);
  const [showStateText, setShowStateText] = useState(false);
  const [copied, setCopied] = useState(false);

  // Compute which sources and targets are valid
  const validSources = useMemo(() => {
    if (!snapshot) return new Set<MoveSource>();
    return new Set(snapshot.legalMoves.map(m => m.source));
  }, [snapshot]);

  const validTargets = useMemo(() => {
    if (!snapshot || selectedSource === null) return new Set<MoveTarget>();
    return new Set(
      snapshot.legalMoves
        .filter(m => m.source === selectedSource)
        .map(m => m.target)
    );
  }, [snapshot, selectedSource]);

  const getBestReward = (): number => {
    if (!analysis || analysis.length === 0) return 0;
    return Math.max(...analysis.map(a => a.reward));
  };

  if (isLoading || !snapshot) {
    return <div style={{ padding: 40, textAlign: 'center', color: '#6b7280' }}>Loading WASM engine...</div>;
  }

  const isMyTurn = snapshot.currentPlayer === 0 && !snapshot.isGameOver && !isAIThinking;

  const handleSourceClick = (source: MoveSource) => {
    if (!isMyTurn) return;
    if (!validSources.has(source)) return;
    if (selectedSource === source) {
      setSelectedSource(null);
    } else {
      setSelectedSource(source);
    }
  };

  const handleTargetClick = (target: MoveTarget) => {
    if (!isMyTurn || selectedSource === null) return;
    if (!validTargets.has(target)) return;
    playMove(selectedSource, target);
    setSelectedSource(null);
  };

  const board = (
    <div style={{ flex: '0 0 auto', maxWidth: 700 }}>
      {/* Header */}
      <div style={{
        display: 'flex', justifyContent: 'space-between', alignItems: 'center',
        marginBottom: 16, paddingBottom: 12, borderBottom: '1px solid #e5e7eb',
      }}>
        <h1 style={{ margin: 0, fontSize: '24px', fontWeight: 700 }}>Skip-Bo</h1>
        <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
          {DIFFICULTY_PRESETS.map((p) => {
            const active = aiType === p.aiType
              && (p.aiType === 'heuristic' || (mctsConfig.iterations === p.config.iterations
              && mctsConfig.determinizations === p.config.determinizations));
            return (
              <button
                key={p.label}
                onClick={() => { setAIType(p.aiType); setMctsConfig(p.config); }}
                style={{
                  padding: '4px 10px', borderRadius: 4, cursor: 'pointer',
                  border: '1px solid #d1d5db',
                  backgroundColor: active ? '#dbeafe' : '#fff',
                  fontWeight: active ? 600 : 400, fontSize: '13px',
                }}
              >
                {p.label}
              </button>
            );
          })}
          <button
            onClick={toggleAnalysis}
            style={{
              padding: '4px 12px', borderRadius: 4, cursor: 'pointer',
              border: '1px solid #d1d5db',
              backgroundColor: showAnalysis ? '#dbeafe' : '#fff',
            }}
          >
            {showAnalysis ? 'Hide Analysis' : 'Show Analysis'}
          </button>
          <button
            onClick={() => {
              setShowStateText(v => !v);
              setCopied(false);
            }}
            style={{
              padding: '4px 12px', borderRadius: 4, cursor: 'pointer',
              border: '1px solid #d1d5db',
              backgroundColor: showStateText ? '#dbeafe' : '#fff',
              fontSize: '13px',
            }}
          >
            Copy State
          </button>
          <button
            onClick={newGame}
            style={{
              padding: '4px 12px', borderRadius: 4, cursor: 'pointer',
              border: '1px solid #d1d5db', backgroundColor: '#fff',
            }}
          >
            New Game
          </button>
        </div>
      </div>

      {/* Status */}
      <div style={{
        textAlign: 'center', marginBottom: 16, padding: '8px',
        backgroundColor: snapshot.isGameOver ? '#dcfce7' : isAIThinking ? '#fef3c7' : '#f3f4f6',
        borderRadius: 8, fontSize: '14px', fontWeight: 500,
      }}>
        {snapshot.isGameOver ? (
          snapshot.winner === 0 ? 'You win!' : 'AI wins!'
        ) : isAIThinking ? (
          'AI is thinking...'
        ) : isMyTurn ? (
          selectedSource !== null ? 'Click a target pile' : 'Click a card to play'
        ) : 'Waiting...'}
      </div>

      {/* Copyable state text */}
      {showStateText && (
        <div style={{ marginBottom: 16, position: 'relative' }}>
          <textarea
            readOnly
            value={snapshotToText(snapshot)}
            style={{
              width: '100%', height: 260, fontFamily: 'monospace', fontSize: '12px',
              padding: '8px', borderRadius: 6, border: '1px solid #d1d5db',
              backgroundColor: '#f9fafb', resize: 'vertical', boxSizing: 'border-box',
            }}
            onFocus={e => e.target.select()}
          />
          <button
            onClick={() => {
              navigator.clipboard.writeText(snapshotToText(snapshot));
              setCopied(true);
              setTimeout(() => setCopied(false), 2000);
            }}
            style={{
              position: 'absolute', top: 8, right: 8, padding: '3px 10px',
              borderRadius: 4, border: '1px solid #d1d5db', cursor: 'pointer',
              backgroundColor: copied ? '#dcfce7' : '#fff', fontSize: '12px',
            }}
          >
            {copied ? 'Copied!' : 'Copy'}
          </button>
        </div>
      )}

      {/* Opponent area */}
      <div style={{
        display: 'flex', gap: 12, justifyContent: 'center', alignItems: 'flex-end',
        marginBottom: 20, padding: '12px', backgroundColor: '#f9fafb', borderRadius: 8,
      }}>
        <PileView
          topCard={snapshot.stockTop[1]}
          size={snapshot.stockSize[1]}
          label="AI Stock"
          faceDown={false}
        />
        <div style={{ width: 16 }} />
        {[0, 1, 2, 3].map(d => (
          <DiscardPileView
            key={`opp-d-${d}`}
            cards={snapshot.discardPileCards[1][d]}
            label={`D${d + 1}`}
          />
        ))}
      </div>

      {/* Building piles (center) */}
      <div style={{
        display: 'flex', gap: 16, justifyContent: 'center',
        marginBottom: 20, padding: '16px',
        backgroundColor: '#ecfdf5', borderRadius: 8, border: '2px solid #a7f3d0',
      }}>
        {[0, 1, 2, 3].map(b => {
          const target = b as MoveTarget;
          const isValid = validTargets.has(target);
          return (
            <PileView
              key={`bp-${b}`}
              topCard={snapshot.buildingPiles[b]}
              size={snapshot.buildingPileSizes[b]}
              label={`Build ${b + 1}`}
              highlighted={isValid}
              onClick={isValid ? () => handleTargetClick(target) : undefined}
            />
          );
        })}
      </div>

      {/* Player area */}
      <div style={{
        padding: '16px', backgroundColor: '#eff6ff', borderRadius: 8,
        border: '2px solid #bfdbfe',
      }}>
        {/* Stock + Discard piles */}
        <div style={{
          display: 'flex', gap: 12, justifyContent: 'center', alignItems: 'flex-end',
          marginBottom: 16,
        }}>
          <PileView
            topCard={snapshot.stockTop[0]}
            size={snapshot.stockSize[0]}
            label="Your Stock"
            highlighted={selectedSource === MoveSource.StockPile || (selectedSource === null && validSources.has(MoveSource.StockPile))}
            onClick={validSources.has(MoveSource.StockPile) ? () => handleSourceClick(MoveSource.StockPile) : undefined}
          />
          <div style={{ width: 16 }} />
          {[0, 1, 2, 3].map(d => {
            const sourceEnum = (MoveSource.DiscardPile0 + d) as MoveSource;
            const targetEnum = (MoveTarget.DiscardPile0 + d) as MoveTarget;
            const isSource = validSources.has(sourceEnum);
            const isTarget = validTargets.has(targetEnum);
            return (
              <DiscardPileView
                key={`my-d-${d}`}
                cards={snapshot.discardPileCards[0][d]}
                label={`D${d + 1}`}
                highlighted={selectedSource === sourceEnum || isTarget}
                onClick={
                  isTarget ? () => handleTargetClick(targetEnum) :
                  isSource ? () => handleSourceClick(sourceEnum) :
                  undefined
                }
              />
            );
          })}
        </div>

        {/* Hand */}
        <div style={{ display: 'flex', gap: 10, justifyContent: 'center' }}>
          {snapshot.hand.map((card, i) => {
            const source = i as MoveSource;
            const isValid = validSources.has(source);
            const isSelected = selectedSource === source;
            return (
              <CardView
                key={`hand-${i}`}
                card={card}
                highlighted={isSelected || (selectedSource === null && isValid)}
                onClick={isValid ? () => handleSourceClick(source) : undefined}
              />
            );
          })}
        </div>
      </div>
    </div>
  );

  const analysisPanel = showAnalysis ? (
    <div style={{
      width: 280, flexShrink: 0, padding: '12px',
      backgroundColor: '#faf5ff', borderRadius: 8, border: '1px solid #e9d5ff',
      alignSelf: 'flex-start',
    }}>
      {/* Skip-Bo tally */}
      <div style={{
        fontSize: '13px', fontWeight: 600, marginBottom: 8, color: '#7c3aed',
      }}>
        Skip-Bo Cards Played
      </div>
      <div style={{
        display: 'flex', gap: 16, marginBottom: 16, paddingBottom: 12,
        borderBottom: '1px solid #e9d5ff', fontSize: '13px',
      }}>
        <div>
          <span style={{ color: '#6b7280' }}>You: </span>
          <span style={{ fontWeight: 700, color: '#d4a017' }}>{snapshot.skipBoPlayed[0]}</span>
        </div>
        <div>
          <span style={{ color: '#6b7280' }}>AI: </span>
          <span style={{ fontWeight: 700, color: '#d4a017' }}>{snapshot.skipBoPlayed[1]}</span>
        </div>
        <div style={{ color: '#9ca3af', fontSize: '11px', marginLeft: 'auto' }}>
          / 18 total
        </div>
      </div>

      {/* Move analysis */}
      <div style={{ fontSize: '13px', fontWeight: 600, marginBottom: 8, color: '#7c3aed' }}>
        Move Analysis (reward)
      </div>
      {analysis && analysis.length > 0 ? (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
          {[...analysis]
            .sort((a, b) => b.reward - a.reward)
            .slice(0, 10)
            .map((a, i) => {
              const best = getBestReward();
              const isBest = Math.abs(a.reward - best) < 0.001;
              const rewardStr = (a.reward >= 0 ? '+' : '') + a.reward.toFixed(2);
              const color = a.reward > 0.5 ? '#16a34a' : a.reward > 0 ? '#65a30d' : a.reward > -0.5 ? '#ca8a04' : '#dc2626';
              return (
                <div key={i} style={{
                  display: 'flex', alignItems: 'center', gap: 6,
                  fontSize: '12px', fontWeight: isBest ? 700 : 400,
                  color: isBest ? '#7c3aed' : '#4b5563',
                }}>
                  <span style={{ width: 120, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                    {sourceLabel(a.source, snapshot)} → {targetLabel(a.target, snapshot)}
                  </span>
                  <span style={{ width: 50, textAlign: 'right', fontSize: '12px', fontWeight: 600, color, fontVariantNumeric: 'tabular-nums' }}>
                    {rewardStr}
                  </span>
                </div>
              );
            })}
        </div>
      ) : (
        <div style={{ fontSize: '12px', color: '#9ca3af' }}>
          {isAnalyzing ? 'Analyzing...' : isMyTurn ? 'Waiting for data' : 'Play a move to see analysis'}
        </div>
      )}

      {/* MCTS config */}
      <div style={{
        marginTop: 16, paddingTop: 12, borderTop: '1px solid #e9d5ff',
      }}>
        <div style={{ fontSize: '13px', fontWeight: 600, marginBottom: 8, color: '#7c3aed' }}>
          MCTS Config
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 6, fontSize: '12px' }}>
          <label style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <span style={{ width: 80, color: '#6b7280' }}>Iterations</span>
            <input
              type="number"
              min={10}
              max={50000}
              step={100}
              value={mctsConfig.iterations}
              onChange={e => setMctsConfig({ ...mctsConfig, iterations: Number(e.target.value) })}
              style={{
                width: 80, padding: '2px 6px', borderRadius: 4,
                border: '1px solid #d1d5db', fontSize: '12px',
              }}
            />
          </label>
          <label style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <span style={{ width: 80, color: '#6b7280' }}>Dets</span>
            <input
              type="number"
              min={1}
              max={100}
              value={mctsConfig.determinizations}
              onChange={e => setMctsConfig({ ...mctsConfig, determinizations: Number(e.target.value) })}
              style={{
                width: 80, padding: '2px 6px', borderRadius: 4,
                border: '1px solid #d1d5db', fontSize: '12px',
              }}
            />
          </label>
          <label style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <span style={{ width: 80, color: '#6b7280' }}>Heuristic %</span>
            <input
              type="number"
              min={0}
              max={100}
              step={10}
              value={mctsConfig.heuristicPct}
              onChange={e => setMctsConfig({ ...mctsConfig, heuristicPct: Number(e.target.value) })}
              style={{
                width: 80, padding: '2px 6px', borderRadius: 4,
                border: '1px solid #d1d5db', fontSize: '12px',
              }}
            />
          </label>
          <label style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <span style={{ width: 80, color: '#6b7280' }}>Tree Depth</span>
            <input
              type="number"
              min={1}
              max={20}
              value={mctsConfig.treeDepth}
              onChange={e => setMctsConfig({ ...mctsConfig, treeDepth: Number(e.target.value) })}
              style={{
                width: 80, padding: '2px 6px', borderRadius: 4,
                border: '1px solid #d1d5db', fontSize: '12px',
              }}
            />
          </label>
          <label style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <span style={{ width: 80, color: '#6b7280' }}>Rollout Depth</span>
            <input
              type="number"
              min={1}
              max={50}
              value={mctsConfig.rolloutDepth}
              onChange={e => setMctsConfig({ ...mctsConfig, rolloutDepth: Number(e.target.value) })}
              style={{
                width: 80, padding: '2px 6px', borderRadius: 4,
                border: '1px solid #d1d5db', fontSize: '12px',
              }}
            />
          </label>
          <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <div style={{ color: '#9ca3af', fontSize: '11px' }}>
              {(mctsConfig.iterations * mctsConfig.determinizations).toLocaleString()} total sims
            </div>
            <button
              onClick={reanalyze}
              disabled={isAnalyzing || !isMyTurn}
              style={{
                marginLeft: 'auto', padding: '3px 10px', borderRadius: 4,
                cursor: isAnalyzing || !isMyTurn ? 'default' : 'pointer',
                border: '1px solid #c084fc',
                backgroundColor: isAnalyzing ? '#f3e8ff' : '#7c3aed',
                color: isAnalyzing ? '#9ca3af' : '#fff',
                fontSize: '12px', fontWeight: 600,
              }}
            >
              {isAnalyzing ? 'Running...' : 'Re-analyze'}
            </button>
          </div>
        </div>
      </div>
    </div>
  ) : null;

  return (
    <div style={{
      display: 'flex', gap: 24, justifyContent: 'center', padding: '16px',
      fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif',
    }}>
      {board}
      {analysisPanel}
    </div>
  );
}
