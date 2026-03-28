import type { EpochEvent } from './types';

interface Props {
  data: EpochEvent[];
  width?: number;
  height?: number;
}

export function LossCurve({ data, width = 400, height = 200 }: Props) {
  if (data.length === 0) {
    return (
      <div style={{
        width, height, border: '1px solid #e5e7eb', borderRadius: 4,
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        color: '#9ca3af', fontSize: 13,
      }}>
        No training data yet
      </div>
    );
  }

  const pad = { top: 20, right: 80, bottom: 30, left: 50 };
  const w = width - pad.left - pad.right;
  const h = height - pad.top - pad.bottom;

  const maxEpoch = Math.max(...data.map((d) => d.epoch));
  const allLosses = data.flatMap((d) => [d.policy_loss, d.value_loss]);
  const maxLoss = Math.max(...allLosses) * 1.1;
  const minLoss = Math.min(0, Math.min(...allLosses));

  const x = (epoch: number) => pad.left + (epoch / maxEpoch) * w;
  const y = (loss: number) => pad.top + h - ((loss - minLoss) / (maxLoss - minLoss)) * h;

  const polyLine = (values: number[], color: string) => {
    if (values.length < 2) return null;
    const points = data.map((d, i) => `${x(d.epoch)},${y(values[i])}`).join(' ');
    return <polyline points={points} fill="none" stroke={color} strokeWidth={2} />;
  };

  const policyValues = data.map((d) => d.policy_loss);
  const valueValues = data.map((d) => d.value_loss);

  // Y-axis ticks
  const numTicks = 5;
  const ticks = Array.from({ length: numTicks + 1 }, (_, i) =>
    minLoss + (maxLoss - minLoss) * (i / numTicks)
  );

  return (
    <svg width={width} height={height} style={{ border: '1px solid #e5e7eb', borderRadius: 4, background: '#fff' }}>
      {/* Grid lines */}
      {ticks.map((t, i) => (
        <g key={i}>
          <line x1={pad.left} y1={y(t)} x2={pad.left + w} y2={y(t)}
            stroke="#f3f4f6" strokeWidth={1} />
          <text x={pad.left - 6} y={y(t) + 4} textAnchor="end" fontSize={10} fill="#9ca3af">
            {t.toFixed(2)}
          </text>
        </g>
      ))}

      {/* X-axis labels */}
      <text x={pad.left + w / 2} y={height - 4} textAnchor="middle" fontSize={10} fill="#9ca3af">
        Epoch
      </text>
      <text x={pad.left} y={height - 4} textAnchor="start" fontSize={10} fill="#9ca3af">1</text>
      <text x={pad.left + w} y={height - 4} textAnchor="end" fontSize={10} fill="#9ca3af">
        {maxEpoch}
      </text>

      {/* Lines */}
      {polyLine(policyValues, '#7c3aed')}
      {polyLine(valueValues, '#059669')}

      {/* Legend */}
      <line x1={pad.left + w + 10} y1={pad.top + 10} x2={pad.left + w + 25} y2={pad.top + 10}
        stroke="#7c3aed" strokeWidth={2} />
      <text x={pad.left + w + 28} y={pad.top + 14} fontSize={10} fill="#6b7280">Policy</text>

      <line x1={pad.left + w + 10} y1={pad.top + 28} x2={pad.left + w + 25} y2={pad.top + 28}
        stroke="#059669" strokeWidth={2} />
      <text x={pad.left + w + 28} y={pad.top + 32} fontSize={10} fill="#6b7280">Value</text>
    </svg>
  );
}
