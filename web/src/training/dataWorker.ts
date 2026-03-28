// Web Worker that generates training data by running logged matches via WASM.
// Messages in:  { type: 'generate', p0Type, p0Iters, p0Dets, p0Tree,
//                 p1Type, p1Iters, p1Dets, p1Tree, numGames, maxChains, startSeed }
// Messages out: { type: 'gameData', gameIndex, records: TrainingRecord[] }
//               { type: 'progress', completed, total }
//               { type: 'done' }
//               { type: 'error', message }

const STATE_SIZE = 158;
const CHAIN_SIZE = 29;

interface WasmVectorInt {
  size(): number;
  get(i: number): number;
  delete(): void;
}

interface WasmModule {
  runMatchLogged(
    p0Type: number, p0Iters: number, p0Dets: number, p0Tree: number,
    p1Type: number, p1Iters: number, p1Dets: number, p1Tree: number,
    seed: number, maxChains: number
  ): WasmVectorInt;
}

let modulePromise: Promise<WasmModule> | null = null;

async function getModule(): Promise<WasmModule> {
  if (!modulePromise) {
    modulePromise = (async () => {
      const resp = await fetch('/wasm/skipbo.js');
      const text = await resp.text();
      (0, eval)(text);
      const factory = (globalThis as unknown as {
        createSkipBoModule: (opts?: object) => Promise<WasmModule>;
      }).createSkipBoModule;
      return await factory({ locateFile: (path: string) => `/wasm/${path}` });
    })();
  }
  return modulePromise;
}

interface TrainingRecord {
  state: number[];
  chains: number[][];
  chosen: number;
  outcome: number;
}

function parseLoggedMatch(v: WasmVectorInt): TrainingRecord[] {
  const records: TrainingRecord[] = [];
  let idx = 0;
  const numTurns = v.get(idx++);

  for (let t = 0; t < numTurns; t++) {
    // State (158 ints, float * 10000)
    const state: number[] = [];
    for (let i = 0; i < STATE_SIZE; i++) {
      state.push(v.get(idx++) / 10000);
    }

    // Chains
    const numChains = v.get(idx++);
    const chains: number[][] = [];
    for (let c = 0; c < numChains; c++) {
      const chain: number[] = [];
      for (let i = 0; i < CHAIN_SIZE; i++) {
        chain.push(v.get(idx++) / 10000);
      }
      chains.push(chain);
    }

    // Chosen index and outcome
    const chosen = v.get(idx++);
    const outcome = v.get(idx++) / 10000;

    records.push({ state, chains, chosen, outcome });
  }

  v.delete();
  return records;
}

self.onmessage = async (e: MessageEvent) => {
  if (e.data.type === 'generate') {
    try {
      const module = await getModule();
      const {
        p0Type, p0Iters, p0Dets, p0Tree,
        p1Type, p1Iters, p1Dets, p1Tree,
        numGames, maxChains, startSeed,
      } = e.data;

      for (let g = 0; g < numGames; g++) {
        const seed = startSeed + g;
        const v = module.runMatchLogged(
          p0Type, p0Iters, p0Dets, p0Tree,
          p1Type, p1Iters, p1Dets, p1Tree,
          seed, maxChains
        );
        const records = parseLoggedMatch(v);

        (self as unknown as Worker).postMessage({
          type: 'gameData',
          gameIndex: g,
          records,
        });

        (self as unknown as Worker).postMessage({
          type: 'progress',
          completed: g + 1,
          total: numGames,
        });
      }

      (self as unknown as Worker).postMessage({ type: 'done' });
    } catch (err) {
      (self as unknown as Worker).postMessage({ type: 'error', message: String(err) });
    }
  }
};
