import { CardView } from './CardView';

interface DiscardPileViewProps {
  cards: number[];  // bottom to top
  label: string;
  highlighted?: boolean;
  onClick?: () => void;
}

export function DiscardPileView({ cards, label, highlighted, onClick }: DiscardPileViewProps) {
  if (cards.length === 0) {
    return (
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4 }}>
        <span style={{ fontSize: '11px', color: '#6b7280', fontWeight: 500 }}>{label}</span>
        <CardView card={-1} onClick={onClick} highlighted={highlighted} />
      </div>
    );
  }

  // Show all cards fanned vertically, top card fully visible
  const cardOffset = 18;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4 }}>
      <span style={{ fontSize: '11px', color: '#6b7280', fontWeight: 500 }}>{label}</span>
      <div style={{
        position: 'relative',
        width: 64,
        height: 96 + (cards.length - 1) * cardOffset,
      }}>
        {cards.map((card, i) => {
          const isTop = i === cards.length - 1;
          return (
            <div
              key={i}
              style={{
                position: 'absolute',
                top: i * cardOffset,
                left: 0,
                zIndex: i,
              }}
            >
              <CardView
                card={card}
                onClick={isTop ? onClick : undefined}
                highlighted={isTop && highlighted}
                topAlign={!isTop}
              />
            </div>
          );
        })}
      </div>
    </div>
  );
}
