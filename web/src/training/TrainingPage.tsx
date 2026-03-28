import { useState, useEffect, useCallback } from 'react';
import { DatasetPanel } from './DatasetPanel';
import { TrainingPanel } from './TrainingPanel';
import { GenerationGallery } from './GenerationGallery';
import type { DatasetMeta, GenerationMeta } from './types';
import * as api from './api';

interface Props {
  onLoadWeights: (name: string, valueWeights: number[], policyWeights: number[]) => void;
  activeGeneration: string | null;
}

export function TrainingPage({ onLoadWeights, activeGeneration }: Props) {
  const [datasets, setDatasets] = useState<DatasetMeta[]>([]);
  const [generations, setGenerations] = useState<GenerationMeta[]>([]);
  const [selectedIds, setSelectedIds] = useState<Set<string>>(new Set());
  const [serverOk, setServerOk] = useState<boolean | null>(null);

  const refreshDatasets = useCallback(async () => {
    try {
      const ds = await api.listDatasets();
      setDatasets(ds);
    } catch {
      // Server might be down
    }
  }, []);

  const refreshGenerations = useCallback(async () => {
    try {
      const gens = await api.listGenerations();
      setGenerations(gens);
    } catch {
      // Server might be down
    }
  }, []);

  const checkServer = useCallback(async () => {
    try {
      await api.healthCheck();
      setServerOk(true);
      refreshDatasets();
      refreshGenerations();
    } catch {
      setServerOk(false);
    }
  }, [refreshDatasets, refreshGenerations]);

  useEffect(() => {
    checkServer();
  }, [checkServer]);

  const toggleDataset = (id: string) => {
    setSelectedIds((prev) => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      return next;
    });
  };

  const loadGeneration = async (name: string) => {
    try {
      const weights = await api.getWeights(name);
      onLoadWeights(name, weights.value_network, weights.policy_network);
    } catch (e) {
      console.error('Failed to load weights:', e);
    }
  };

  if (serverOk === null) {
    return (
      <div style={{ padding: 24, textAlign: 'center', color: '#6b7280' }}>
        Connecting to training server...
      </div>
    );
  }

  if (!serverOk) {
    return (
      <div style={{ padding: 24, maxWidth: 500, margin: '0 auto' }}>
        <div style={{
          padding: 16, backgroundColor: '#fef2f2', border: '1px solid #fecaca',
          borderRadius: 8, marginBottom: 16,
        }}>
          <div style={{ fontWeight: 600, color: '#dc2626', marginBottom: 4 }}>
            Training server not running
          </div>
          <div style={{ fontSize: 13, color: '#6b7280' }}>
            Start the Python server to enable neural network training:
          </div>
          <pre style={{
            marginTop: 8, padding: 8, backgroundColor: '#fff', borderRadius: 4,
            fontSize: 12, overflowX: 'auto',
          }}>
{`cd server
pip install -r requirements.txt
uvicorn server.main:app --reload`}
          </pre>
        </div>
        <button
          onClick={checkServer}
          style={{
            padding: '6px 16px', fontSize: 13, backgroundColor: '#7c3aed',
            color: '#fff', border: 'none', borderRadius: 4, cursor: 'pointer',
          }}>
          Retry Connection
        </button>
      </div>
    );
  }

  return (
    <div style={{ padding: 24, maxWidth: 900, margin: '0 auto' }}>
      <div style={{ marginBottom: 24 }}>
        <DatasetPanel
          datasets={datasets}
          selectedIds={selectedIds}
          onToggle={toggleDataset}
          onRefresh={refreshDatasets}
        />
      </div>

      <div style={{ marginBottom: 24, borderTop: '1px solid #e5e7eb', paddingTop: 20 }}>
        <TrainingPanel
          selectedDatasetIds={selectedIds}
          generations={generations}
          onTrainingComplete={() => {
            refreshGenerations();
          }}
        />
      </div>

      <div style={{ borderTop: '1px solid #e5e7eb', paddingTop: 20 }}>
        <GenerationGallery
          generations={generations}
          activeGeneration={activeGeneration}
          onLoad={loadGeneration}
          onRefresh={refreshGenerations}
        />
      </div>
    </div>
  );
}
