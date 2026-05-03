"""
Actor-Critic networks for PPO.
Action space: 80 (10 sources x 8 targets) with masking.
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

from .encoding import STATE_SIZE
from .game_env import NUM_ACTIONS

HIDDEN1 = 256
HIDDEN2 = 128


class ActorCritic(nn.Module):
    """Separate actor and critic networks for PPO."""

    def __init__(self):
        super().__init__()
        # Actor (policy)
        self.actor_fc1 = nn.Linear(STATE_SIZE, HIDDEN1)
        self.actor_fc2 = nn.Linear(HIDDEN1, HIDDEN2)
        self.actor_fc3 = nn.Linear(HIDDEN2, NUM_ACTIONS)

        # Critic (value)
        self.critic_fc1 = nn.Linear(STATE_SIZE, HIDDEN1)
        self.critic_fc2 = nn.Linear(HIDDEN1, HIDDEN2)
        self.critic_fc3 = nn.Linear(HIDDEN2, 1)

        # Orthogonal init (standard for PPO)
        for m in [self.actor_fc1, self.actor_fc2, self.critic_fc1, self.critic_fc2]:
            nn.init.orthogonal_(m.weight, gain=np.sqrt(2))
            nn.init.zeros_(m.bias)
        nn.init.orthogonal_(self.actor_fc3.weight, gain=0.01)
        nn.init.zeros_(self.actor_fc3.bias)
        nn.init.orthogonal_(self.critic_fc3.weight, gain=1.0)
        nn.init.zeros_(self.critic_fc3.bias)

    def forward(self, state: torch.Tensor, action_mask: torch.Tensor):
        """
        Args:
            state: (batch, 158)
            action_mask: (batch, 80) boolean mask, True = legal

        Returns:
            logits: (batch, 80) masked logits
            value: (batch, 1)
        """
        # Actor
        a = F.relu(self.actor_fc1(state))
        a = F.relu(self.actor_fc2(a))
        logits = self.actor_fc3(a)

        # Mask illegal actions with -inf
        logits = logits.masked_fill(~action_mask, float("-inf"))

        # Critic
        c = F.relu(self.critic_fc1(state))
        c = F.relu(self.critic_fc2(c))
        value = torch.tanh(self.critic_fc3(c))

        return logits, value

    def get_action_and_value(
        self, state: torch.Tensor, action_mask: torch.Tensor, action: torch.Tensor | None = None
    ):
        """
        Sample or evaluate an action.

        Returns:
            action, log_prob, entropy, value
        """
        logits, value = self.forward(state, action_mask)
        dist = torch.distributions.Categorical(logits=logits)

        if action is None:
            action = dist.sample()

        log_prob = dist.log_prob(action)
        entropy = dist.entropy()

        return action, log_prob, entropy, value.squeeze(-1)


def export_weights_for_cpp(model: ActorCritic, path: str | Path):
    """Export actor weights as flat JSON for C++ inference."""
    actor_weights: list[float] = []
    for name, param in model.named_parameters():
        if name.startswith("actor_"):
            actor_weights.extend(param.detach().cpu().numpy().flatten().tolist())

    critic_weights: list[float] = []
    for name, param in model.named_parameters():
        if name.startswith("critic_"):
            critic_weights.extend(param.detach().cpu().numpy().flatten().tolist())

    data = {
        "type": "ppo",
        "actor_network": actor_weights,
        "critic_network": critic_weights,
        "architecture": {
            "input_size": STATE_SIZE,
            "hidden1": HIDDEN1,
            "hidden2": HIDDEN2,
            "output_size": NUM_ACTIONS,
        },
    }

    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f)
    print(f"Exported PPO weights to {path} ({len(actor_weights)} actor params, {len(critic_weights)} critic params)")
