// WASM module loader and typed wrapper

export interface SkipBoModule {
  GameController: new (seed: number) => GameController;
}

export interface VectorInt {
  size(): number;
  get(i: number): number;
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
  getLegalMoves(): VectorInt;
  applyMove(source: number, target: number): boolean;
  playAITurn(iterations: number, determinizations: number): VectorInt;
  passTurn(): void;
  analyzeMoves(iterations: number, determinizations: number): VectorInt;
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
