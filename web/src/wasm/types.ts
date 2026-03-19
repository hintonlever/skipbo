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
  winProbability: number;
}

export interface GameSnapshot {
  hand: number[];
  stockTop: [number, number];     // [player0, player1]
  stockSize: [number, number];
  buildingPiles: number[];        // top cards (4 piles), -1 if empty
  buildingPileSizes: number[];
  discardPiles: number[][];       // [player][pile] = top card
  discardPileSizes: number[][];
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
      winProbability: flat[i + 2] / 1000,
    });
  }
  return result;
}
