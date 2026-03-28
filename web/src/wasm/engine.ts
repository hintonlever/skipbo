// WASM module loader and typed wrapper

export interface SkipBoModule {
  GameController: new (seed: number) => GameController;
  VectorFloat: new () => VectorFloat;
  runMatch(
    p0Type: number, p0Iters: number, p0Dets: number, p0Heuristic: number, p0Rollout: number, p0Tree: number,
    p1Type: number, p1Iters: number, p1Dets: number, p1Heuristic: number, p1Rollout: number, p1Tree: number,
    seed: number
  ): VectorInt;
  runMatchLogged(
    p0Type: number, p0Iters: number, p0Dets: number, p0Tree: number,
    p1Type: number, p1Iters: number, p1Dets: number, p1Tree: number,
    seed: number, maxChains: number
  ): VectorInt;
}

export interface VectorInt {
  size(): number;
  get(i: number): number;
  delete(): void;
}

export interface VectorFloat {
  size(): number;
  get(i: number): number;
  push_back(v: number): void;
  delete(): void;
}

export interface GameController {
  isGameOver(): boolean;
  getWinner(): number;
  getCurrentPlayer(): number;
  getHand(): VectorInt;
  getStockTop(player: number): number;
  getStockSize(player: number): number;
  getBuildingPileTop(pile: number): number;
  getBuildingPileSize(pile: number): number;
  getDiscardTop(player: number, pile: number): number;
  getDiscardSize(player: number, pile: number): number;
  getDiscardPile(player: number, pile: number): VectorInt;
  getSkipBoPlayed(player: number): number;
  getLegalMoves(): VectorInt;
  applyMove(source: number, target: number): boolean;
  playAITurn(iterations: number, determinizations: number, turnDepth: number): VectorInt;
  playHeuristicAITurn(): VectorInt;
  passTurn(): void;
  analyzeMoves(iterations: number, determinizations: number, turnDepth: number): VectorInt;
  analyzeChains(iterations: number, determinizations: number, turnDepth: number): VectorInt;
  getMoveTree(): VectorInt;
  // Neural network
  loadNNWeights(valueWeights: VectorFloat, policyWeights: VectorFloat): void;
  hasNNWeights(): boolean;
  playNNAITurn(iterations: number, determinizations: number, turnDepth: number, cpuct: number): VectorInt;
  analyzeNNChains(iterations: number, determinizations: number, turnDepth: number, cpuct: number): VectorInt;
  delete(): void;
}

// Declare the global factory function loaded by the script tag
declare global {
  interface Window {
    createSkipBoModule: () => Promise<SkipBoModule>;
  }
}

let modulePromise: Promise<SkipBoModule> | null = null;

function loadScript(src: string): Promise<void> {
  return new Promise((resolve, reject) => {
    if (document.querySelector(`script[src="${src}"]`)) {
      resolve();
      return;
    }
    const script = document.createElement('script');
    script.src = src;
    script.onload = () => resolve();
    script.onerror = () => reject(new Error(`Failed to load ${src}`));
    document.head.appendChild(script);
  });
}

export function initEngine(): Promise<SkipBoModule> {
  if (!modulePromise) {
    modulePromise = (async () => {
      await loadScript('/wasm/skipbo.js');
      const module = await window.createSkipBoModule();
      return module;
    })();
  }
  return modulePromise;
}

// Helper to convert VectorInt to JS array and clean up
export function vectorToArray(v: VectorInt): number[] {
  const arr: number[] = [];
  for (let i = 0; i < v.size(); i++) {
    arr.push(v.get(i));
  }
  v.delete();
  return arr;
}

// Helper to create a VectorFloat from a JS array (for passing weights to WASM)
export function arrayToVectorFloat(module: SkipBoModule, arr: number[]): VectorFloat {
  const v = new module.VectorFloat();
  for (const val of arr) v.push_back(val);
  return v;
}
