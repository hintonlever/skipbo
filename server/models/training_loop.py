from __future__ import annotations

import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Generator

import numpy as np
import torch
import torch.nn.functional as F
from torch.utils.data import Dataset, DataLoader

from .networks import ValueNetwork, PolicyNetwork, export_weights_flat, count_parameters

STATE_SIZE = 158
CHAIN_SIZE = 29


@dataclass
class TrainingConfig:
    epochs: int = 50
    batch_size: int = 256
    learning_rate: float = 0.001
    weight_decay: float = 0.0001
    value_weight: float = 1.0
    policy_weight: float = 1.0


@dataclass
class EpochResult:
    epoch: int
    policy_loss: float
    value_loss: float
    total_loss: float


class SkipBoDataset(Dataset):
    """Load training data from .npz batch files."""

    def __init__(self, dataset_dirs: list[Path]):
        self.states: list[np.ndarray] = []
        self.chains: list[np.ndarray] = []
        self.chain_masks: list[np.ndarray] = []
        self.chosen: list[int] = []
        self.outcomes: list[float] = []

        for d in dataset_dirs:
            games_dir = d / "games"
            if not games_dir.exists():
                continue
            for f in sorted(games_dir.glob("batch_*.npz")):
                data = np.load(f)
                n = len(data["outcome"])
                for i in range(n):
                    state = data["state"][i]  # (STATE_SIZE,)
                    chains_i = data["chains"][i]  # (max_chains, CHAIN_SIZE)
                    mask_i = data["chain_mask"][i]  # (max_chains,)
                    chosen_i = int(data["chosen"][i])
                    outcome_i = float(data["outcome"][i])

                    # Skip records where chosen chain wasn't matched
                    if chosen_i < 0:
                        continue
                    # Skip if chosen index is out of valid range
                    if chosen_i >= int(mask_i.sum()):
                        continue

                    self.states.append(state)
                    self.chains.append(chains_i)
                    self.chain_masks.append(mask_i)
                    self.chosen.append(chosen_i)
                    self.outcomes.append(outcome_i)

    def __len__(self) -> int:
        return len(self.states)

    def __getitem__(self, idx: int):
        return {
            "state": torch.tensor(self.states[idx], dtype=torch.float32),
            "chains": torch.tensor(self.chains[idx], dtype=torch.float32),
            "chain_mask": torch.tensor(self.chain_masks[idx], dtype=torch.bool),
            "chosen": torch.tensor(self.chosen[idx], dtype=torch.long),
            "outcome": torch.tensor(self.outcomes[idx], dtype=torch.float32),
        }


def train(
    dataset_dirs: list[Path],
    generation_name: str,
    output_dir: Path,
    config: TrainingConfig,
    stop_flag: list[bool] | None = None,
) -> Generator[EpochResult, None, None]:
    """Train value and policy networks. Yields EpochResult per epoch."""

    device = torch.device("cpu")  # Skip-Bo networks are tiny, CPU is fine

    dataset = SkipBoDataset(dataset_dirs)
    print(f"[training] Loaded {len(dataset)} valid training samples from {len(dataset_dirs)} dataset(s)")
    if len(dataset) == 0:
        print("[training] No valid samples — check that chosen_idx != -1 in the data")
        return

    loader = DataLoader(
        dataset,
        batch_size=config.batch_size,
        shuffle=True,
        drop_last=False,
    )

    value_net = ValueNetwork().to(device)
    policy_net = PolicyNetwork().to(device)

    optimizer = torch.optim.Adam(
        list(value_net.parameters()) + list(policy_net.parameters()),
        lr=config.learning_rate,
        weight_decay=config.weight_decay,
    )

    for epoch in range(config.epochs):
        if stop_flag and stop_flag[0]:
            break

        total_policy_loss = 0.0
        total_value_loss = 0.0
        total_samples = 0

        value_net.train()
        policy_net.train()

        for batch in loader:
            states = batch["state"].to(device)  # (B, 158)
            chains = batch["chains"].to(device)  # (B, max_chains, 29)
            mask = batch["chain_mask"].to(device)  # (B, max_chains)
            chosen = batch["chosen"].to(device)  # (B,)
            outcome = batch["outcome"].to(device)  # (B,)

            B, max_chains, _ = chains.shape

            # Value loss: MSE between predicted and actual outcome
            value_pred = value_net(states).squeeze(-1)  # (B,)
            v_loss = F.mse_loss(value_pred, outcome)

            # Policy loss: cross-entropy over valid chains
            # Score each chain: expand state to match chains
            states_exp = states.unsqueeze(1).expand(B, max_chains, STATE_SIZE)  # (B, mc, 158)
            states_flat = states_exp.reshape(B * max_chains, STATE_SIZE)
            chains_flat = chains.reshape(B * max_chains, CHAIN_SIZE)
            scores = policy_net(states_flat, chains_flat).reshape(B, max_chains)  # (B, mc)

            # Mask invalid chains with large negative value
            scores = scores.masked_fill(~mask, -1e9)

            # Cross-entropy loss with chosen chain as target
            p_loss = F.cross_entropy(scores, chosen)

            loss = config.value_weight * v_loss + config.policy_weight * p_loss

            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

            total_policy_loss += p_loss.item() * B
            total_value_loss += v_loss.item() * B
            total_samples += B

        avg_p = total_policy_loss / max(total_samples, 1)
        avg_v = total_value_loss / max(total_samples, 1)

        result = EpochResult(
            epoch=epoch + 1,
            policy_loss=avg_p,
            value_loss=avg_v,
            total_loss=avg_p + avg_v,
        )
        yield result

    # Save models and export weights
    gen_dir = output_dir / generation_name
    gen_dir.mkdir(parents=True, exist_ok=True)

    torch.save(value_net.state_dict(), gen_dir / "model_value.pt")
    torch.save(policy_net.state_dict(), gen_dir / "model_policy.pt")

    weights = {
        "generation": generation_name,
        "value_network": export_weights_flat(value_net),
        "policy_network": export_weights_flat(policy_net),
        "metadata": {
            "value_params": count_parameters(value_net),
            "policy_params": count_parameters(policy_net),
            "training_samples": len(dataset),
            "epochs": config.epochs,
        },
    }
    with open(gen_dir / "weights.json", "w") as f:
        json.dump(weights, f)

    meta = {
        "name": generation_name,
        "value_params": count_parameters(value_net),
        "policy_params": count_parameters(policy_net),
        "training_samples": len(dataset),
        "epochs": config.epochs,
        "final_policy_loss": result.policy_loss,
        "final_value_loss": result.value_loss,
    }
    with open(gen_dir / "meta.json", "w") as f:
        json.dump(meta, f, indent=2)
