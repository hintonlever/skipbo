import type { GenerationMeta } from './types';
import * as api from './api';

interface Props {
  generations: GenerationMeta[];
  activeGeneration: string | null;
  onLoad: (name: string) => void;
  onRefresh: () => void;
}

export function GenerationGallery({ generations, activeGeneration, onLoad, onRefresh }: Props) {
  const handleDelete = async (name: string) => {
    await api.deleteGeneration(name);
    onRefresh();
  };

  return (
    <div>
      <h3 style={{ margin: '0 0 12px', fontSize: 15, fontWeight: 600 }}>Generations</h3>
      {generations.length === 0 ? (
        <div style={{ fontSize: 13, color: '#9ca3af' }}>No trained generations yet</div>
      ) : (
        <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap' }}>
          {generations.map((gen) => {
            const isActive = activeGeneration === gen.name;
            return (
              <div key={gen.name} style={{
                width: 160, padding: 12, borderRadius: 8,
                border: isActive ? '2px solid #7c3aed' : '1px solid #e5e7eb',
                backgroundColor: isActive ? '#f5f3ff' : '#fff',
              }}>
                <div style={{ fontWeight: 600, fontSize: 16, marginBottom: 4 }}>
                  {gen.name}
                </div>
                <div style={{ fontSize: 11, color: '#6b7280', lineHeight: 1.6 }}>
                  <div>
                    {((gen.value_params + gen.policy_params) / 1000).toFixed(1)}K params
                  </div>
                  <div>{gen.training_samples.toLocaleString()} samples</div>
                  <div>{gen.epochs} epochs</div>
                  <div>P-Loss: {gen.final_policy_loss?.toFixed(4) ?? '?'}</div>
                  <div>V-Loss: {gen.final_value_loss?.toFixed(4) ?? '?'}</div>
                </div>
                <div style={{ display: 'flex', gap: 4, marginTop: 8 }}>
                  {isActive ? (
                    <span style={{
                      padding: '2px 10px', fontSize: 11, fontWeight: 600,
                      color: '#7c3aed', backgroundColor: '#ede9fe',
                      borderRadius: 4,
                    }}>Active</span>
                  ) : (
                    <button
                      onClick={() => onLoad(gen.name)}
                      style={{
                        padding: '2px 10px', fontSize: 11, fontWeight: 600,
                        color: '#fff', backgroundColor: '#7c3aed',
                        border: 'none', borderRadius: 4, cursor: 'pointer',
                      }}>
                      Load
                    </button>
                  )}
                  <button
                    onClick={() => handleDelete(gen.name)}
                    style={{
                      padding: '2px 8px', fontSize: 11, color: '#ef4444',
                      backgroundColor: 'transparent', border: '1px solid #fca5a5',
                      borderRadius: 4, cursor: 'pointer',
                    }}>
                    x
                  </button>
                </div>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
}
