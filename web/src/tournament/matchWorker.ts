// Web Worker that loads the WASM module and runs matches.
// Messages in:  { type: 'run', jobs: MatchJob[] }
// Messages out: { type: 'result', jobIndex: number, winner: number, turns: number }
//               { type: 'error', message: string }

export interface MatchJob {
  jobIndex: number;
  p0Type: number; p0Iters: number; p0Dets: number; p0Heuristic: number; p0Rollout: number; p0Tree: number;
  p1Type: number; p1Iters: number; p1Dets: number; p1Heuristic: number; p1Rollout: number; p1Tree: number;
  seed: number;
}

interface WasmVectorInt {
  size(): number;
  get(i: number): number;
  delete(): void;
}

interface WasmVectorFloat {
  size(): number;
  get(i: number): number;
  push_back(v: number): void;
  delete(): void;
}

interface WasmModule {
  VectorFloat: new () => WasmVectorFloat;
  runMatch(
    p0Type: number, p0Iters: number, p0Dets: number, p0Heuristic: number, p0Rollout: number, p0Tree: number,
    p1Type: number, p1Iters: number, p1Dets: number, p1Heuristic: number, p1Rollout: number, p1Tree: number,
    seed: number
  ): WasmVectorInt;
  loadNNWeightsGlobal(valueWeights: WasmVectorFloat, policyWeights: WasmVectorFloat): void;
  hasNNWeightsGlobal(): boolean;
}

let modulePromise: Promise<WasmModule> | null = null;

async function getModule(): Promise<WasmModule> {
  if (!modulePromise) {
    modulePromise = (async () => {
      // In a module worker, importScripts is not available.
      // Fetch the script text and eval it to define createSkipBoModule on globalThis.
      const resp = await fetch('/wasm/skipbo.js');
      const text = await resp.text();
      // eslint-disable-next-line no-eval
      (0, eval)(text);
      const factory = (globalThis as unknown as { createSkipBoModule: (opts?: object) => Promise<WasmModule> }).createSkipBoModule;
      return await factory({
        locateFile: (path: string) => `/wasm/${path}`,
      });
    })();
  }
  return modulePromise;
}

self.onmessage = async (e: MessageEvent) => {
  if (e.data.type === 'loadWeights') {
    try {
      const module = await getModule();
      const vw = new module.VectorFloat();
      const pw = new module.VectorFloat();
      for (const v of e.data.valueWeights as number[]) vw.push_back(v);
      for (const v of e.data.policyWeights as number[]) pw.push_back(v);
      module.loadNNWeightsGlobal(vw, pw);
      vw.delete();
      pw.delete();
      (self as unknown as Worker).postMessage({ type: 'weightsLoaded' });
    } catch (err) {
      (self as unknown as Worker).postMessage({ type: 'error', message: String(err) });
    }
  } else if (e.data.type === 'run') {
    try {
      const module = await getModule();
      const jobs: MatchJob[] = e.data.jobs;

      for (const job of jobs) {
        const v = module.runMatch(
          job.p0Type, job.p0Iters, job.p0Dets, job.p0Heuristic, job.p0Rollout, job.p0Tree,
          job.p1Type, job.p1Iters, job.p1Dets, job.p1Heuristic, job.p1Rollout, job.p1Tree,
          job.seed
        );
        const winner = v.get(0);
        const turns = v.get(1);
        v.delete();

        (self as unknown as Worker).postMessage({ type: 'result', jobIndex: job.jobIndex, winner, turns });
      }
    } catch (err) {
      (self as unknown as Worker).postMessage({ type: 'error', message: String(err) });
    }
  }
};
