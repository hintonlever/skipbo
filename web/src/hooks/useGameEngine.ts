import { useState, useCallback, useEffect, useRef } from 'react';
import { initEngine, vectorToArray, type GameController } from '../wasm/engine';
import {
  type GameSnapshot, type MoveWithAnalysis,
  MoveSource, MoveTarget, parseMovePairs, parseAnalysis, isDiscard
} from '../wasm/types';

function takeSnapshot(ctrl: GameController): GameSnapshot {
  const hand = vectorToArray(ctrl.getHand());
  const legalFlat = vectorToArray(ctrl.getLegalMoves());

  return {
    hand,
    stockTop: [ctrl.getStockTop(0), ctrl.getStockTop(1)],
    stockSize: [ctrl.getStockSize(0), ctrl.getStockSize(1)],
    buildingPiles: [0, 1, 2, 3].map(i => ctrl.getBuildingPileTop(i)),
    buildingPileSizes: [0, 1, 2, 3].map(i => ctrl.getBuildingPileSize(i)),
    discardPiles: [0, 1].map(p => [0, 1, 2, 3].map(d => ctrl.getDiscardTop(p, d))),
    discardPileSizes: [0, 1].map(p => [0, 1, 2, 3].map(d => ctrl.getDiscardSize(p, d))),
    currentPlayer: ctrl.getCurrentPlayer(),
    isGameOver: ctrl.isGameOver(),
    winner: ctrl.getWinner(),
    legalMoves: parseMovePairs(legalFlat),
  };
}

export interface GameEngine {
  snapshot: GameSnapshot | null;
  isLoading: boolean;
  isAIThinking: boolean;
  aiDifficulty: number;
  analysis: MoveWithAnalysis[] | null;
  showAnalysis: boolean;
  playMove: (source: MoveSource, target: MoveTarget) => void;
  newGame: () => void;
  setAiDifficulty: (d: number) => void;
  toggleAnalysis: () => void;
}

export function useGameEngine(): GameEngine {
  const ctrlRef = useRef<GameController | null>(null);
  const [snapshot, setSnapshot] = useState<GameSnapshot | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [isAIThinking, setIsAIThinking] = useState(false);
  const [aiDifficulty, setAiDifficulty] = useState(1); // 0=easy, 1=medium, 2=hard
  const [analysis, setAnalysis] = useState<MoveWithAnalysis[] | null>(null);
  const [showAnalysis, setShowAnalysis] = useState(false);

  const difficultyConfigs = [
    { iterations: 50, determinizations: 3 },    // easy
    { iterations: 100, determinizations: 5 },   // medium
    { iterations: 300, determinizations: 10 },   // hard
  ];

  const refreshSnapshot = useCallback(() => {
    if (ctrlRef.current) {
      setSnapshot(takeSnapshot(ctrlRef.current));
      setAnalysis(null);
    }
  }, []);

  const startGame = useCallback(async () => {
    setIsLoading(true);
    try {
      const module = await initEngine();
      if (ctrlRef.current) {
        ctrlRef.current.delete();
      }
      const seed = Math.floor(Math.random() * 1000000);
      ctrlRef.current = new module.GameController(seed);
      refreshSnapshot();
    } finally {
      setIsLoading(false);
    }
  }, [refreshSnapshot]);

  useEffect(() => {
    startGame();
    return () => {
      if (ctrlRef.current) {
        ctrlRef.current.delete();
        ctrlRef.current = null;
      }
    };
  }, [startGame]);

  const runAITurn = useCallback(() => {
    const ctrl = ctrlRef.current;
    if (!ctrl || ctrl.isGameOver() || ctrl.getCurrentPlayer() !== 1) return;

    setIsAIThinking(true);

    // Use setTimeout to let UI update before blocking on AI computation
    setTimeout(() => {
      const config = difficultyConfigs[aiDifficulty];
      vectorToArray(ctrl.playAITurn(config.iterations, config.determinizations));
      setIsAIThinking(false);
      refreshSnapshot();
    }, 50);
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [aiDifficulty, refreshSnapshot]);

  const playMove = useCallback((source: MoveSource, target: MoveTarget) => {
    const ctrl = ctrlRef.current;
    if (!ctrl || ctrl.isGameOver() || ctrl.getCurrentPlayer() !== 0) return;

    const ok = ctrl.applyMove(source, target);
    if (!ok) return;

    refreshSnapshot();

    // If this was a discard (ends turn) or game is over, check if AI should play
    if (isDiscard(target) && !ctrl.isGameOver()) {
      runAITurn();
    }
  }, [refreshSnapshot, runAITurn]);

  const runAnalysis = useCallback(() => {
    const ctrl = ctrlRef.current;
    if (!ctrl || ctrl.isGameOver() || ctrl.getCurrentPlayer() !== 0) return;

    const config = difficultyConfigs[aiDifficulty];
    const flat = vectorToArray(ctrl.analyzeMoves(config.iterations, config.determinizations));
    setAnalysis(parseAnalysis(flat));
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [aiDifficulty]);

  const toggleAnalysis = useCallback(() => {
    setShowAnalysis(prev => {
      const next = !prev;
      if (next) runAnalysis();
      else setAnalysis(null);
      return next;
    });
  }, [runAnalysis]);

  return {
    snapshot,
    isLoading,
    isAIThinking,
    aiDifficulty,
    analysis,
    showAnalysis,
    playMove,
    newGame: startGame,
    setAiDifficulty,
    toggleAnalysis,
  };
}
