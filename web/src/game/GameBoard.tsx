import { useState, useMemo } from 'react';
import { CardView } from './CardView';
import { PileView } from './PileView';
import { type GameEngine } from '../hooks/useGameEngine';
import { MoveSource, MoveTarget } from '../wasm/types';

interface GameBoardProps {
  engine: GameEngine;
}

function sourceLabel(s: MoveSource): string {
  if (s <= MoveSource.Hand4) return `Hand ${s}`;
  if (s === MoveSource.StockPile) return 'Stock';
  return `Discard ${s - MoveSource.DiscardPile0}`;
}

function targetLabel(t: MoveTarget): string {
  if (t <= MoveTarget.BuildingPile3) return `Build ${t}`;
  return `Discard ${t - MoveTarget.DiscardPile0}`;
}

export function GameBoard({ engine }: GameBoardProps) {
  const { snapshot, isAIThinking, playMove, newGame, aiDifficulty, setAiDifficulty,
    analysis, showAnalysis, toggleAnalysis, isLoading } = engine;
  const [selectedSource, setSelectedSource] = useState<MoveSource | null>(null);

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

  const getBestWinProb = (): number => {
    if (!analysis || analysis.length === 0) return 0;
    return Math.max(...analysis.map(a => a.winProbability));
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

  return (
    <div style={{
      maxWidth: 700, margin: '0 auto', padding: '16px',
      fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif',
    }}>
      {/* Header */}
      <div style={{
        display: 'flex', justifyContent: 'space-between', alignItems: 'center',
        marginBottom: 16, paddingBottom: 12, borderBottom: '1px solid #e5e7eb',
      }}>
        <h1 style={{ margin: 0, fontSize: '24px', fontWeight: 700 }}>Skip-Bo</h1>
        <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
          <select
            value={aiDifficulty}
            onChange={e => setAiDifficulty(Number(e.target.value))}
            style={{ padding: '4px 8px', borderRadius: 4, border: '1px solid #d1d5db' }}
          >
            <option value={0}>Easy AI</option>
            <option value={1}>Medium AI</option>
            <option value={2}>Hard AI</option>
          </select>
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
          <PileView
            key={`opp-d-${d}`}
            topCard={snapshot.discardPiles[1][d]}
            size={snapshot.discardPileSizes[1][d]}
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
              <PileView
                key={`my-d-${d}`}
                topCard={snapshot.discardPiles[0][d]}
                size={snapshot.discardPileSizes[0][d]}
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

      {/* Analysis panel */}
      {showAnalysis && analysis && analysis.length > 0 && (
        <div style={{
          marginTop: 16, padding: '12px', backgroundColor: '#faf5ff',
          borderRadius: 8, border: '1px solid #e9d5ff',
        }}>
          <div style={{ fontSize: '13px', fontWeight: 600, marginBottom: 8, color: '#7c3aed' }}>
            Move Analysis (win %)
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
            {[...analysis]
              .sort((a, b) => b.winProbability - a.winProbability)
              .slice(0, 10)
              .map((a, i) => {
                const best = getBestWinProb();
                const isBest = Math.abs(a.winProbability - best) < 0.001;
                const pct = (a.winProbability * 100).toFixed(1);
                return (
                  <div key={i} style={{
                    display: 'flex', alignItems: 'center', gap: 8,
                    fontSize: '12px', fontWeight: isBest ? 700 : 400,
                    color: isBest ? '#7c3aed' : '#4b5563',
                  }}>
                    <span style={{ width: 150, whiteSpace: 'nowrap' }}>
                      {sourceLabel(a.source)} → {targetLabel(a.target)}
                    </span>
                    <div style={{
                      flex: 1, height: 14, backgroundColor: '#e5e7eb', borderRadius: 4,
                      overflow: 'hidden',
                    }}>
                      <div style={{
                        width: `${a.winProbability * 100}%`,
                        height: '100%',
                        backgroundColor: a.winProbability > 0.5 ? '#22c55e' : a.winProbability > 0.3 ? '#eab308' : '#ef4444',
                        borderRadius: 4,
                        transition: 'width 0.3s',
                      }} />
                    </div>
                    <span style={{ width: 48, textAlign: 'right' }}>{pct}%</span>
                  </div>
                );
              })}
          </div>
        </div>
      )}
    </div>
  );
}
