// Card values matching C++ engine
export const CARD_SKIPBO = 0;
export const CARD_NONE = -1; // returned by getStockTop/getDiscardTop when empty

// Move sources (matching C++ MoveSource enum)
export const MoveSource = {
  Hand0: 0, Hand1: 1, Hand2: 2, Hand3: 3, Hand4: 4,
  StockPile: 5,
  DiscardPile0: 6, DiscardPile1: 7, DiscardPile2: 8, DiscardPile3: 9,
} as const;
export type MoveSource = typeof MoveSource[keyof typeof MoveSource];

// Move targets (matching C++ MoveTarget enum)
export const MoveTarget = {
  BuildingPile0: 0, BuildingPile1: 1, BuildingPile2: 2, BuildingPile3: 3,
  DiscardPile0: 4, DiscardPile1: 5, DiscardPile2: 6, DiscardPile3: 7,
} as const;
export type MoveTarget = typeof MoveTarget[keyof typeof MoveTarget];

export interface Move {
  source: MoveSource;
  target: MoveTarget;
}

export interface MoveWithAnalysis extends Move {
  reward: number; // avg MCTS reward (net stock progress), can be negative
}

export interface GameSnapshot {
  hand: number[];
  stockTop: [number, number];     // [player0, player1]
  stockSize: [number, number];
  buildingPiles: number[];        // top cards (4 piles), -1 if empty
  buildingPileSizes: number[];
  discardPiles: number[][];       // [player][pile] = top card
  discardPileSizes: number[][];
  discardPileCards: number[][][];  // [player][pile] = array of cards bottom-to-top
  skipBoPlayed: [number, number];  // [player0, player1] total SB cards played
  currentPlayer: number;
  isGameOver: boolean;
  winner: number;
  legalMoves: Move[];
}

export function cardToString(card: number): string {
  if (card === CARD_NONE || card < 0) return '';
  if (card === CARD_SKIPBO) return 'SB';
  return String(card);
}

export function isDiscard(target: MoveTarget): boolean {
  return target >= MoveTarget.DiscardPile0;
}

export function parseMovePairs(flat: number[]): Move[] {
  const moves: Move[] = [];
  for (let i = 0; i < flat.length; i += 2) {
    moves.push({ source: flat[i] as MoveSource, target: flat[i + 1] as MoveTarget });
  }
  return moves;
}

export function parseAnalysis(flat: number[]): MoveWithAnalysis[] {
  const result: MoveWithAnalysis[] = [];
  for (let i = 0; i < flat.length; i += 3) {
    result.push({
      source: flat[i] as MoveSource,
      target: flat[i + 1] as MoveTarget,
      reward: flat[i + 2] / 1000,
    });
  }
  return result;
}

// Chain analysis: a complete turn action (builds + discard) with MCTS evaluation
export interface ChainAnalysis {
  moves: Move[];      // ordered sequence: build moves then discard
  visits: number;
  reward: number;     // avg reward from root player's perspective
}

// Parse flat chain array from WASM: [numChains, numMoves1, src, tgt, ..., visits, reward*1000, ...]
export function parseChains(flat: number[]): ChainAnalysis[] {
  if (flat.length === 0) return [];
  const numChains = flat[0];
  const result: ChainAnalysis[] = [];
  let idx = 1;
  for (let c = 0; c < numChains && idx < flat.length; c++) {
    const numMoves = flat[idx++];
    const moves: Move[] = [];
    for (let m = 0; m < numMoves && idx + 1 < flat.length; m++) {
      moves.push({ source: flat[idx] as MoveSource, target: flat[idx + 1] as MoveTarget });
      idx += 2;
    }
    const visits = flat[idx++];
    const reward = flat[idx++] / 1000;
    result.push({ moves, visits, reward });
  }
  return result;
}

// MCTS tree node for visualization
export interface TreeNode {
  index: number;
  parentIndex: number;
  source: number;
  target: number;
  visits: number;
  avgReward: number;  // always from root player's perspective
  actingPlayer: number;  // 0 = you, 1 = opponent
  children: TreeNode[];
}

// Parse flat tree array [parentIdx, source, target, visits, avgReward*1000, actingPlayer, ...] into a tree
export function parseTree(flat: number[]): TreeNode | null {
  if (flat.length < 6) return null;

  const nodes: TreeNode[] = [];
  for (let i = 0; i < flat.length; i += 6) {
    nodes.push({
      index: i / 6,
      parentIndex: flat[i],
      source: flat[i + 1],
      target: flat[i + 2],
      visits: flat[i + 3],
      avgReward: flat[i + 4] / 1000,
      actingPlayer: flat[i + 5],
      children: [],
    });
  }

  // Build tree by linking children to parents
  for (let i = 1; i < nodes.length; i++) {
    const parent = nodes[nodes[i].parentIndex];
    if (parent) parent.children.push(nodes[i]);
  }

  return nodes[0];
}
