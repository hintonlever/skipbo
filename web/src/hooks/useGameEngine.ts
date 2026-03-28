import { useState, useCallback, useEffect, useRef } from 'react';
import { initEngine, vectorToArray, arrayToVectorFloat, type GameController, type SkipBoModule } from '../wasm/engine';
import {
  type GameSnapshot, type ChainAnalysis, type MoveTreeNode,
  MoveSource, MoveTarget, parseMovePairs, parseChains, parseMoveTree, isDiscard
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
  turnDepth: number; // max turns per player in the tree
}

export const DIFFICULTY_PRESETS: { label: string; aiType: AIType; config: MCTSConfig }[] = [
  { label: 'Heuristic', aiType: 'heuristic', config: { iterations: 0, determinizations: 0, turnDepth: 0 } },
  { label: 'Easy',   aiType: 'mcts', config: { iterations: 100,   determinizations: 5,  turnDepth: 1 } },
  { label: 'Medium', aiType: 'mcts', config: { iterations: 500,   determinizations: 10, turnDepth: 2 } },
  { label: 'Hard',   aiType: 'mcts', config: { iterations: 2000,  determinizations: 20, turnDepth: 2 } },
];

export interface GameEngine {
  snapshot: GameSnapshot | null;
  isLoading: boolean;
  isAIThinking: boolean;
  isAnalyzing: boolean;
  aiType: AIType;
  mctsConfig: MCTSConfig;
  chains: ChainAnalysis[] | null;
  moveTree: MoveTreeNode | null;
  showAnalysis: boolean;
  activeGeneration: string | null;
  playMove: (source: MoveSource, target: MoveTarget) => void;
  newGame: () => void;
  setAIType: (type: AIType) => void;
  setMctsConfig: (config: MCTSConfig) => void;
  toggleAnalysis: () => void;
  reanalyze: () => void;
  refreshMoveTree: () => void;
  loadNNWeights: (name: string, valueWeights: number[], policyWeights: number[]) => Promise<void>;
}

export function useGameEngine(): GameEngine {
  const ctrlRef = useRef<GameController | null>(null);
  const [snapshot, setSnapshot] = useState<GameSnapshot | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [isAIThinking, setIsAIThinking] = useState(false);
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [aiType, setAIType] = useState<AIType>(DIFFICULTY_PRESETS[2].aiType);
  const [mctsConfig, setMctsConfig] = useState<MCTSConfig>(DIFFICULTY_PRESETS[2].config);
  const [chains, setChains] = useState<ChainAnalysis[] | null>(null);
  const [moveTree, setMoveTree] = useState<MoveTreeNode | null>(null);
  const [showAnalysis, setShowAnalysis] = useState(false);
  const [activeGeneration, setActiveGeneration] = useState<string | null>(null);
  const moduleRef = useRef<SkipBoModule | null>(null);
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
      try {
        const cfg = mctsConfigRef.current;
        const flat = vectorToArray(ctrl.analyzeChains(cfg.iterations, cfg.determinizations, cfg.turnDepth));
        setChains(parseChains(flat));
      } catch (e) {
        console.error('analyzeChains failed:', e);
        setChains([]);
      }
      setIsAnalyzing(false);
    }, 30);
  }, []);

  const refreshMoveTree = useCallback(() => {
    const ctrl = ctrlRef.current;
    if (!ctrl || ctrl.isGameOver() || ctrl.getCurrentPlayer() !== 0) {
      setMoveTree(null);
      return;
    }
    try {
      const flat = vectorToArray(ctrl.getMoveTree());
      setMoveTree(parseMoveTree(flat));
    } catch (e) {
      console.error('getMoveTree failed:', e);
      setMoveTree(null);
    }
  }, []);

  const refreshSnapshot = useCallback((andAnalyze: boolean = false) => {
    if (ctrlRef.current) {
      setSnapshot(takeSnapshot(ctrlRef.current));
      setChains(null);
      setMoveTree(null);
      if (andAnalyze || showAnalysisRef.current) {
        runAnalysis();
      }
    }
  }, [runAnalysis]);

  const startGame = useCallback(async () => {
    setIsLoading(true);
    try {
      const module = await initEngine();
      moduleRef.current = module;
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
      try {
        if (aiTypeRef.current === 'heuristic') {
          vectorToArray(ctrl.playHeuristicAITurn());
        } else {
          const cfg = mctsConfigRef.current;
          vectorToArray(ctrl.playAITurn(cfg.iterations, cfg.determinizations, cfg.turnDepth));
        }
      } catch (e) {
        console.error('AI turn failed:', e);
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

  const loadNNWeights = useCallback(async (name: string, valueWeights: number[], policyWeights: number[]) => {
    const ctrl = ctrlRef.current;
    const module = moduleRef.current;
    if (!ctrl || !module) return;
    const vw = arrayToVectorFloat(module, valueWeights);
    const pw = arrayToVectorFloat(module, policyWeights);
    ctrl.loadNNWeights(vw, pw);
    vw.delete();
    pw.delete();
    setActiveGeneration(name);
  }, []);

  const toggleAnalysis = useCallback(() => {
    setShowAnalysis(prev => {
      const next = !prev;
      if (next) {
        runAnalysis();
      } else {
        setChains(null);
      }
      return next;
    });
  }, [runAnalysis]);

  return {
    snapshot,
    isLoading,
    isAIThinking,
    isAnalyzing,
    aiType,
    mctsConfig,
    chains,
    moveTree,
    showAnalysis,
    activeGeneration,
    playMove,
    newGame: startGame,
    setAIType,
    setMctsConfig,
    toggleAnalysis,
    reanalyze: runAnalysis,
    refreshMoveTree,
    loadNNWeights,
  };
}
