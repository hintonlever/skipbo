import { useState } from 'react';
import { GameBoard } from './game/GameBoard';
import { MCTSTreeView } from './game/MCTSTreeView';
import { TournamentPage } from './tournament/TournamentPage';
import { useGameEngine } from './hooks/useGameEngine';

type Page = 'game' | 'tree' | 'tournament';

function App() {
  const engine = useGameEngine();
  const [page, setPage] = useState<Page>('game');

  const tabStyle = (active: boolean) => ({
    padding: '6px 16px',
    cursor: 'pointer' as const,
    border: 'none',
    borderBottom: active ? '2px solid #7c3aed' : '2px solid transparent',
    backgroundColor: 'transparent',
    fontWeight: active ? 600 : 400,
    color: active ? '#7c3aed' : '#6b7280',
    fontSize: '14px',
  });

  return (
    <div style={{ minHeight: '100vh', backgroundColor: '#f8fafc' }}>
      <div style={{
        display: 'flex', justifyContent: 'center', gap: 4,
        borderBottom: '1px solid #e5e7eb', backgroundColor: '#fff',
      }}>
        <button style={tabStyle(page === 'game')} onClick={() => setPage('game')}>
          Game
        </button>
        <button style={tabStyle(page === 'tree')} onClick={() => setPage('tree')}>
          MCTS Tree
        </button>
        <button style={tabStyle(page === 'tournament')} onClick={() => setPage('tournament')}>
          Tournament
        </button>
      </div>

      {page === 'game' && <GameBoard engine={engine} />}
      {page === 'tree' && <MCTSTreeView engine={engine} />}
      {page === 'tournament' && <TournamentPage />}
    </div>
  );
}

export default App;
