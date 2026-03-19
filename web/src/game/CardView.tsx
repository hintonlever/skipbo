import { cardToString, CARD_SKIPBO } from '../wasm/types';

interface CardViewProps {
  card: number;
  faceDown?: boolean;
  onClick?: () => void;
  highlighted?: boolean;
  small?: boolean;
  label?: string;
}

function cardColor(card: number): string {
  if (card === CARD_SKIPBO) return '#d4a017';
  if (card >= 1 && card <= 4) return '#2563eb';
  if (card >= 5 && card <= 8) return '#16a34a';
  if (card >= 9 && card <= 12) return '#dc2626';
  return '#6b7280';
}

export function CardView({ card, faceDown, onClick, highlighted, small, label }: CardViewProps) {
  const isEmpty = card < 0;
  const w = small ? 48 : 64;
  const h = small ? 72 : 96;
  const fontSize = small ? '14px' : '20px';

  if (isEmpty && !label) {
    return (
      <div
        onClick={onClick}
        style={{
          width: w, height: h,
          border: '2px dashed #d1d5db',
          borderRadius: 8,
          display: 'flex', alignItems: 'center', justifyContent: 'center',
          cursor: onClick ? 'pointer' : 'default',
          backgroundColor: highlighted ? '#fef3c7' : 'transparent',
          transition: 'background-color 0.15s',
        }}
      />
    );
  }

  if (faceDown) {
    return (
      <div style={{
        width: w, height: h,
        backgroundColor: '#1e3a5f',
        border: '2px solid #0f2440',
        borderRadius: 8,
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        background: 'repeating-linear-gradient(45deg, #1e3a5f, #1e3a5f 4px, #264d73 4px, #264d73 8px)',
      }}>
        {label && <span style={{ color: '#fff', fontSize: '11px', fontWeight: 600 }}>{label}</span>}
      </div>
    );
  }

  const color = cardColor(card);

  return (
    <div
      onClick={onClick}
      style={{
        width: w, height: h,
        backgroundColor: highlighted ? '#fef9c3' : '#fff',
        border: `3px solid ${highlighted ? '#f59e0b' : color}`,
        borderRadius: 8,
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        cursor: onClick ? 'pointer' : 'default',
        fontWeight: 700,
        fontSize,
        color,
        boxShadow: highlighted ? '0 0 8px rgba(245, 158, 11, 0.5)' : '0 1px 3px rgba(0,0,0,0.12)',
        transition: 'all 0.15s',
        userSelect: 'none',
        position: 'relative',
      }}
    >
      {cardToString(card)}
      {label && (
        <span style={{
          position: 'absolute', bottom: 2, fontSize: '9px',
          color: '#9ca3af', fontWeight: 400,
        }}>{label}</span>
      )}
    </div>
  );
}
