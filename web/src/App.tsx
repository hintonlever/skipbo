import { GameBoard } from './game/GameBoard';
import { useGameEngine } from './hooks/useGameEngine';

function App() {
  const engine = useGameEngine();

  return (
    <div style={{ minHeight: '100vh', backgroundColor: '#f8fafc' }}>
      <GameBoard engine={engine} />
    </div>
  );
}

export default App;
