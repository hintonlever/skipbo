import { useState, useCallback, useEffect, useRef } from 'react';
import { initEngine, vectorToArray, type GameController } from '../wasm/engine';
import {
  type GameSnapshot, type MoveWithAnalysis, type TreeNode,
  MoveSource, MoveTarget, parseMovePairs, parseAnalysis, parseTree, isDiscard
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
    discardPileCards: [0, 1].map(p => [0, 1, 2, 3].map(d => vectorToArray(ctrl.getDiscardPile(p, d)))),
    skipBoPlayed: [ctrl.getSkipBoPlayed(0), ctrl.getSkipBoPlayed(1)],
    currentPlayer: ctrl.getCurrentPlayer(),
    isGameOver: ctrl.isGameOver(),
    winner: ctrl.getWinner(),
    legalMoves: parseMovePairs(legalFlat),
  };
}

export type AIType = 'mcts' | 'heuristic';

export interface MCTSConfig {
  iterations: number;
  determinizations: number;
  heuristicPct: number; // 0-100, rollout heuristic rate
  treeDepth: number; // max depth of MCTS tree (select/expand)
  rolloutDepth: number; // moves per player in rollout
}

export const DIFFICULTY_PRESETS: { label: string; aiType: AIType; config: MCTSConfig }[] = [
  { label: 'Heuristic', aiType: 'heuristic', config: { iterations: 0, determinizations: 0, heuristicPct: 0, treeDepth: 0, rolloutDepth: 0 } },
  { label: 'Easy',   aiType: 'mcts', config: { iterations: 50,    determinizations: 3,  heuristicPct: 50, treeDepth: 5, rolloutDepth: 10 } },
  { label: 'Medium', aiType: 'mcts', config: { iterations: 300,   determinizations: 10, heuristicPct: 50, treeDepth: 5, rolloutDepth: 10 } },
  { label: 'Hard',   aiType: 'mcts', config: { iterations: 10000, determinizations: 20, heuristicPct: 50, treeDepth: 5, rolloutDepth: 10 } },
];

export interface GameEngine {
  snapshot: GameSnapshot | null;
  isLoading: boolean;
  isAIThinking: boolean;
  isAnalyzing: boolean;
  isAnalyzingTree: boolean;
  aiType: AIType;
  mctsConfig: MCTSConfig;
  analysis: MoveWithAnalysis[] | null;
  treeData: TreeNode | null;
  showAnalysis: boolean;
  playMove: (source: MoveSource, target: MoveTarget) => void;
  newGame: () => void;
  setAIType: (type: AIType) => void;
  setMctsConfig: (config: MCTSConfig) => void;
  toggleAnalysis: () => void;
  reanalyze: () => void;
  analyzeTree: () => void;
}

export function useGameEngine(): GameEngine {
  const ctrlRef = useRef<GameController | null>(null);
  const [snapshot, setSnapshot] = useState<GameSnapshot | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [isAIThinking, setIsAIThinking] = useState(false);
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [aiType, setAIType] = useState<AIType>(DIFFICULTY_PRESETS[2].aiType);
  const [mctsConfig, setMctsConfig] = useState<MCTSConfig>(DIFFICULTY_PRESETS[2].config);
  const [analysis, setAnalysis] = useState<MoveWithAnalysis[] | null>(null);
  const [treeData, setTreeData] = useState<TreeNode | null>(null);
  const [isAnalyzingTree, setIsAnalyzingTree] = useState(false);
  const [showAnalysis, setShowAnalysis] = useState(false);
  const showAnalysisRef = useRef(showAnalysis);
  showAnalysisRef.current = showAnalysis;
  const aiTypeRef = useRef(aiType);
  aiTypeRef.current = aiType;
  const mctsConfigRef = useRef(mctsConfig);
  mctsConfigRef.current = mctsConfig;

  const runAnalysis = useCallback(() => {
    const ctrl = ctrlRef.current;
    if (!ctrl || ctrl.isGameOver() || ctrl.getCurrentPlayer() !== 0) return;

    setIsAnalyzing(true);
    setTimeout(() => {
      const cfg = mctsConfigRef.current;
      const flat = vectorToArray(ctrl.analyzeMoves(cfg.iterations, cfg.determinizations, cfg.heuristicPct, cfg.rolloutDepth, cfg.treeDepth));
      setAnalysis(parseAnalysis(flat));
      setIsAnalyzing(false);
    }, 30);
  }, []);

  const runTreeAnalysis = useCallback(() => {
    const ctrl = ctrlRef.current;
    if (!ctrl || ctrl.isGameOver() || ctrl.getCurrentPlayer() !== 0) return;

    setIsAnalyzingTree(true);
    setTimeout(() => {
      const cfg = mctsConfigRef.current;
      const flat = vectorToArray(ctrl.analyzeTree(
        cfg.iterations, cfg.heuristicPct, cfg.rolloutDepth,
        cfg.treeDepth, 3, 10
      ));
      setTreeData(parseTree(flat));
      setIsAnalyzingTree(false);
    }, 30);
  }, []);

  const refreshSnapshot = useCallback((andAnalyze: boolean = false) => {
    if (ctrlRef.current) {
      setSnapshot(takeSnapshot(ctrlRef.current));
      setAnalysis(null);
      setTreeData(null);
      if (andAnalyze || showAnalysisRef.current) {
        runAnalysis();
      }
    }
  }, [runAnalysis]);

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

    setTimeout(() => {
      if (aiTypeRef.current === 'heuristic') {
        vectorToArray(ctrl.playHeuristicAITurn());
      } else {
        const cfg = mctsConfigRef.current;
        vectorToArray(ctrl.playAITurn(cfg.iterations, cfg.determinizations, cfg.heuristicPct, cfg.rolloutDepth, cfg.treeDepth));
      }
      setIsAIThinking(false);
      refreshSnapshot();
    }, 50);
  }, [refreshSnapshot]);

  const playMove = useCallback((source: MoveSource, target: MoveTarget) => {
    const ctrl = ctrlRef.current;
    if (!ctrl || ctrl.isGameOver() || ctrl.getCurrentPlayer() !== 0) return;

    const ok = ctrl.applyMove(source, target);
    if (!ok) return;

    refreshSnapshot();

    if (isDiscard(target) && !ctrl.isGameOver()) {
      runAITurn();
    }
  }, [refreshSnapshot, runAITurn]);

  const toggleAnalysis = useCallback(() => {
    setShowAnalysis(prev => {
      const next = !prev;
      if (next) {
        runAnalysis();
      } else {
        setAnalysis(null);
      }
      return next;
    });
  }, [runAnalysis]);

  return {
    snapshot,
    isLoading,
    isAIThinking,
    isAnalyzing,
    isAnalyzingTree,
    aiType,
    mctsConfig,
    analysis,
    treeData,
    showAnalysis,
    playMove,
    newGame: startGame,
    setAIType,
    setMctsConfig,
    toggleAnalysis,
    reanalyze: runAnalysis,
    analyzeTree: runTreeAnalysis,
  };
}
