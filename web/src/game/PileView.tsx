import { CardView } from './CardView';

interface PileViewProps {
  topCard: number;
  size: number;
  onClick?: () => void;
  highlighted?: boolean;
  faceDown?: boolean;
  label: string;
}

export function PileView({ topCard, size, onClick, highlighted, faceDown, label }: PileViewProps) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4 }}>
      <span style={{ fontSize: '11px', color: '#6b7280', fontWeight: 500 }}>{label}</span>
      <div style={{ position: 'relative' }}>
        <CardView
          card={topCard}
          onClick={onClick}
          highlighted={highlighted}
          faceDown={faceDown && size > 0}
        />
        {size > 0 && (
          <span style={{
            position: 'absolute', top: -6, right: -6,
            backgroundColor: '#374151', color: '#fff',
            borderRadius: '50%', width: 22, height: 22,
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            fontSize: '11px', fontWeight: 600,
          }}>
            {size}
          </span>
        )}
      </div>
    </div>
  );
}
