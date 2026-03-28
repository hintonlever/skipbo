from __future__ import annotations

import torch
import torch.nn as nn
import torch.nn.functional as F

STATE_SIZE = 158
CHAIN_SIZE = 29


class ValueNetwork(nn.Module):
    """State (158) -> win probability in [-1, +1]."""

    def __init__(self, input_size: int = STATE_SIZE, hidden1: int = 128, hidden2: int = 64):
        super().__init__()
        self.fc1 = nn.Linear(input_size, hidden1)
        self.fc2 = nn.Linear(hidden1, hidden2)
        self.fc3 = nn.Linear(hidden2, 1)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = F.relu(self.fc1(x))
        x = F.relu(self.fc2(x))
        return torch.tanh(self.fc3(x))


class PolicyNetwork(nn.Module):
    """State (158) + Chain (29) -> scalar score for that chain."""

    def __init__(
        self,
        state_size: int = STATE_SIZE,
        chain_size: int = CHAIN_SIZE,
        hidden1: int = 128,
        hidden2: int = 64,
    ):
        super().__init__()
        self.fc1 = nn.Linear(state_size + chain_size, hidden1)
        self.fc2 = nn.Linear(hidden1, hidden2)
        self.fc3 = nn.Linear(hidden2, 1)

    def forward(self, state: torch.Tensor, chain: torch.Tensor) -> torch.Tensor:
        x = torch.cat([state, chain], dim=-1)
        x = F.relu(self.fc1(x))
        x = F.relu(self.fc2(x))
        return self.fc3(x)


def export_weights_flat(model: nn.Module) -> list[float]:
    """Export model weights as a flat list matching C++ load format:
    [w1, b1, w2, b2, w3, b3] concatenated."""
    weights: list[float] = []
    for name, param in model.named_parameters():
        weights.extend(param.detach().cpu().numpy().flatten().tolist())
    return weights


def count_parameters(model: nn.Module) -> int:
    return sum(p.numel() for p in model.parameters())
