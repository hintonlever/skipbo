"""
PPO training for Skip-Bo.

Self-play: two copies of the current policy play against each other.
We collect experience from both players' perspectives and train on it.

Usage:
    python -m ppo.train --num-games 1000 --epochs 3 --output ppo_weights.json
"""
from __future__ import annotations

import argparse
import time
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim

from .encoding import encode_state
from .game_env import NUM_ACTIONS, SkipBoEnv, is_discard_action
from .networks import ActorCritic, export_weights_for_cpp


@dataclass
class PPOConfig:
    # Self-play
    num_games_per_batch: int = 256
    num_batches: int = 200

    # PPO hyperparameters
    gamma: float = 0.99
    gae_lambda: float = 0.95
    clip_epsilon: float = 0.2
    entropy_coef: float = 0.01
    value_coef: float = 0.5
    max_grad_norm: float = 0.5

    # Training
    lr: float = 3e-4
    ppo_epochs: int = 4
    minibatch_size: int = 512

    # Opponent pool
    opponent: str = "self"  # "self", "random", "heuristic", "mixed"

    # Output
    output: str = "ppo_weights.json"
    save_every: int = 25
    device: str = "cpu"


@dataclass
class StepRecord:
    state: np.ndarray
    action: int
    action_mask: np.ndarray
    log_prob: float
    value: float
    reward: float
    done: bool


def play_game_collect_experience(
    env: SkipBoEnv,
    model: ActorCritic,
    opponent_model: ActorCritic | None,
    device: torch.device,
) -> tuple[list[StepRecord], list[StepRecord]]:
    """
    Play one game of self-play, collecting experience from both players.
    Returns (player0_steps, player1_steps).
    """
    env.reset()
    experience: tuple[list[StepRecord], list[StepRecord]] = ([], [])

    model.eval()
    if opponent_model is not None:
        opponent_model.eval()

    max_steps = 5000
    steps = 0

    while not env.game_over and steps < max_steps:
        steps += 1
        cp = env.current_player
        legal_actions = env.get_legal_actions()

        if not legal_actions:
            env.pass_turn()
            continue

        state = encode_state(env, cp)
        mask = np.zeros(NUM_ACTIONS, dtype=bool)
        for a in legal_actions:
            mask[a] = True

        # Choose which model to use
        acting_model = model if (cp == 0 or opponent_model is None) else opponent_model

        with torch.no_grad():
            state_t = torch.from_numpy(state).unsqueeze(0).to(device)
            mask_t = torch.from_numpy(mask).unsqueeze(0).to(device)
            action, log_prob, _, value = acting_model.get_action_and_value(state_t, mask_t)

        action_int = action.item()
        lp = log_prob.item()
        val = value.item()

        # Only record experience for the model we're training (player 0 in self-play,
        # or both if opponent_model is None)
        should_record = (opponent_model is None) or (cp == 0)
        if should_record:
            experience[cp].append(
                StepRecord(
                    state=state,
                    action=action_int,
                    action_mask=mask,
                    log_prob=lp,
                    value=val,
                    reward=0.0,  # filled in at end
                    done=False,
                )
            )

        game_over, winner = env.step(action_int)

    if steps >= max_steps and not env.game_over:
        # Force end
        s0 = env.players[0].stock_size()
        s1 = env.players[1].stock_size()
        winner = 0 if s0 <= s1 else 1
        env.game_over = True
        env.winner = winner

    # Assign terminal rewards
    for p in range(2):
        if experience[p]:
            experience[p][-1].done = True
            if winner == p:
                experience[p][-1].reward = 1.0
            else:
                experience[p][-1].reward = -1.0

    return experience


def compute_gae(
    steps: list[StepRecord], gamma: float, lam: float
) -> tuple[np.ndarray, np.ndarray]:
    """Compute GAE advantages and returns."""
    n = len(steps)
    if n == 0:
        return np.array([]), np.array([])

    advantages = np.zeros(n, dtype=np.float32)
    last_gae = 0.0

    for t in reversed(range(n)):
        if steps[t].done:
            next_value = 0.0
        elif t + 1 < n:
            next_value = steps[t + 1].value
        else:
            next_value = 0.0

        delta = steps[t].reward + gamma * next_value * (1 - steps[t].done) - steps[t].value
        last_gae = delta + gamma * lam * (1 - steps[t].done) * last_gae
        advantages[t] = last_gae

    returns = advantages + np.array([s.value for s in steps], dtype=np.float32)
    return advantages, returns


class HeuristicOpponent:
    """Simple heuristic: always play builds if possible, discard lowest card to emptiest pile."""

    def choose_action(self, env: SkipBoEnv) -> int:
        legal = env.get_legal_actions()
        if not legal:
            return -1

        # Prefer build moves (non-discard)
        builds = [a for a in legal if not is_discard_action(a)]
        if builds:
            # Prefer stock plays
            stock_builds = [a for a in builds if a // 8 == 5]
            if stock_builds:
                return stock_builds[0]
            return builds[0]

        # Must discard - pick first discard action
        return legal[0]


def collect_batch(
    config: PPOConfig,
    model: ActorCritic,
    opponent_model: ActorCritic | None,
    device: torch.device,
    heuristic: HeuristicOpponent | None,
) -> tuple[list[StepRecord], dict]:
    """Collect a batch of games worth of experience."""
    all_steps: list[StepRecord] = []
    wins = 0
    losses = 0
    total_game_lengths = 0

    for game_idx in range(config.num_games_per_batch):
        env = SkipBoEnv()
        env.reset()

        if config.opponent == "heuristic" and heuristic is not None:
            # Play as player 0 vs heuristic player 1
            experience_p0: list[StepRecord] = []
            model.eval()
            max_steps = 5000
            steps = 0

            while not env.game_over and steps < max_steps:
                steps += 1
                cp = env.current_player
                legal_actions = env.get_legal_actions()
                if not legal_actions:
                    env.pass_turn()
                    continue

                if cp == 0:
                    state = encode_state(env, 0)
                    mask = np.zeros(NUM_ACTIONS, dtype=bool)
                    for a in legal_actions:
                        mask[a] = True
                    with torch.no_grad():
                        state_t = torch.from_numpy(state).unsqueeze(0).to(device)
                        mask_t = torch.from_numpy(mask).unsqueeze(0).to(device)
                        action, log_prob, _, value = model.get_action_and_value(state_t, mask_t)
                    action_int = action.item()
                    experience_p0.append(
                        StepRecord(
                            state=state,
                            action=action_int,
                            action_mask=mask,
                            log_prob=log_prob.item(),
                            value=value.item(),
                            reward=0.0,
                            done=False,
                        )
                    )
                    env.step(action_int)
                else:
                    action_int = heuristic.choose_action(env)
                    if action_int < 0:
                        env.pass_turn()
                    else:
                        env.step(action_int)

            if steps >= max_steps and not env.game_over:
                s0 = env.players[0].stock_size()
                s1 = env.players[1].stock_size()
                env.winner = 0 if s0 <= s1 else 1
                env.game_over = True

            if experience_p0:
                experience_p0[-1].done = True
                experience_p0[-1].reward = 1.0 if env.winner == 0 else -1.0

            adv, ret = compute_gae(experience_p0, config.gamma, config.gae_lambda)
            for i, step in enumerate(experience_p0):
                step.reward = ret[i]  # overwrite reward with return for convenience
            all_steps.extend(experience_p0)
            total_game_lengths += len(experience_p0)
            if env.winner == 0:
                wins += 1
            else:
                losses += 1

        else:
            # Self-play
            exp = play_game_collect_experience(env, model, opponent_model, device)
            for p in range(2):
                if exp[p]:
                    adv, ret = compute_gae(exp[p], config.gamma, config.gae_lambda)
                    for i, step in enumerate(exp[p]):
                        step.reward = ret[i]
                    all_steps.extend(exp[p])

            total_game_lengths += sum(len(exp[p]) for p in range(2))
            # In self-play, track from player 0 perspective
            if env.winner == 0:
                wins += 1
            else:
                losses += 1

    stats = {
        "games": config.num_games_per_batch,
        "steps": len(all_steps),
        "avg_game_length": total_game_lengths / max(1, config.num_games_per_batch),
        "win_rate": wins / max(1, wins + losses),
    }
    return all_steps, stats


def ppo_update(
    config: PPOConfig,
    model: ActorCritic,
    optimizer: optim.Optimizer,
    steps: list[StepRecord],
    device: torch.device,
) -> dict:
    """Run PPO update epochs on collected experience."""
    if not steps:
        return {}

    # Prepare tensors
    states = torch.from_numpy(np.stack([s.state for s in steps])).to(device)
    actions = torch.tensor([s.action for s in steps], dtype=torch.long, device=device)
    masks = torch.from_numpy(np.stack([s.action_mask for s in steps])).to(device)
    old_log_probs = torch.tensor([s.log_prob for s in steps], dtype=torch.float32, device=device)
    old_values = torch.tensor([s.value for s in steps], dtype=torch.float32, device=device)
    returns = torch.tensor([s.reward for s in steps], dtype=torch.float32, device=device)

    advantages = returns - old_values
    # Normalize advantages
    if len(advantages) > 1:
        advantages = (advantages - advantages.mean()) / (advantages.std() + 1e-8)

    n = len(steps)
    total_pg_loss = 0.0
    total_v_loss = 0.0
    total_entropy = 0.0
    num_updates = 0

    model.train()
    for _ in range(config.ppo_epochs):
        indices = torch.randperm(n, device=device)

        for start in range(0, n, config.minibatch_size):
            end = min(start + config.minibatch_size, n)
            mb_idx = indices[start:end]

            mb_states = states[mb_idx]
            mb_actions = actions[mb_idx]
            mb_masks = masks[mb_idx]
            mb_old_lp = old_log_probs[mb_idx]
            mb_advantages = advantages[mb_idx]
            mb_returns = returns[mb_idx]

            _, new_log_prob, entropy, new_value = model.get_action_and_value(
                mb_states, mb_masks, mb_actions
            )

            # Policy loss (clipped surrogate)
            ratio = torch.exp(new_log_prob - mb_old_lp)
            pg_loss1 = -mb_advantages * ratio
            pg_loss2 = -mb_advantages * torch.clamp(ratio, 1 - config.clip_epsilon, 1 + config.clip_epsilon)
            pg_loss = torch.max(pg_loss1, pg_loss2).mean()

            # Value loss
            v_loss = F.mse_loss(new_value, mb_returns)

            # Entropy bonus
            entropy_loss = -entropy.mean()

            loss = pg_loss + config.value_coef * v_loss + config.entropy_coef * entropy_loss

            optimizer.zero_grad()
            loss.backward()
            nn.utils.clip_grad_norm_(model.parameters(), config.max_grad_norm)
            optimizer.step()

            total_pg_loss += pg_loss.item()
            total_v_loss += v_loss.item()
            total_entropy += entropy.mean().item()
            num_updates += 1

    return {
        "pg_loss": total_pg_loss / max(1, num_updates),
        "v_loss": total_v_loss / max(1, num_updates),
        "entropy": total_entropy / max(1, num_updates),
    }


def train(config: PPOConfig):
    device = torch.device(config.device)
    model = ActorCritic().to(device)
    optimizer = optim.Adam(model.parameters(), lr=config.lr)

    # Opponent for training
    opponent_model = None
    heuristic = None
    if config.opponent == "heuristic":
        heuristic = HeuristicOpponent()
    elif config.opponent == "self":
        opponent_model = None  # same model plays both sides

    total_params = sum(p.numel() for p in model.parameters())
    print(f"PPO Training: {total_params:,} parameters")
    print(f"Config: {config.num_batches} batches x {config.num_games_per_batch} games, "
          f"opponent={config.opponent}")
    print()

    best_win_rate = 0.0

    for batch_idx in range(config.num_batches):
        t0 = time.time()

        # Collect experience
        steps, collect_stats = collect_batch(config, model, opponent_model, device, heuristic)

        # PPO update
        update_stats = ppo_update(config, model, optimizer, steps, device)

        elapsed = time.time() - t0
        wr = collect_stats["win_rate"]
        print(
            f"Batch {batch_idx + 1:4d}/{config.num_batches} | "
            f"WR {wr:.1%} | "
            f"Steps {collect_stats['steps']:6d} | "
            f"AvgLen {collect_stats['avg_game_length']:.0f} | "
            f"PG {update_stats.get('pg_loss', 0):.4f} | "
            f"VL {update_stats.get('v_loss', 0):.4f} | "
            f"Ent {update_stats.get('entropy', 0):.3f} | "
            f"{elapsed:.1f}s"
        )

        # Save periodically
        if (batch_idx + 1) % config.save_every == 0:
            checkpoint_path = Path(config.output).with_suffix(f".batch{batch_idx + 1}.json")
            export_weights_for_cpp(model, checkpoint_path)

        if wr > best_win_rate:
            best_win_rate = wr

    # Final export
    export_weights_for_cpp(model, config.output)
    print(f"\nTraining complete. Best win rate: {best_win_rate:.1%}")
    print(f"Weights saved to {config.output}")

    # Also save PyTorch checkpoint
    torch_path = Path(config.output).with_suffix(".pt")
    torch.save({"model": model.state_dict(), "optimizer": optimizer.state_dict()}, torch_path)
    print(f"PyTorch checkpoint: {torch_path}")


def main():
    parser = argparse.ArgumentParser(description="PPO training for Skip-Bo")
    parser.add_argument("--num-games", type=int, default=256, help="Games per batch")
    parser.add_argument("--num-batches", type=int, default=200, help="Number of training batches")
    parser.add_argument("--lr", type=float, default=3e-4, help="Learning rate")
    parser.add_argument("--ppo-epochs", type=int, default=4, help="PPO epochs per batch")
    parser.add_argument("--minibatch-size", type=int, default=512, help="Minibatch size")
    parser.add_argument("--gamma", type=float, default=0.99, help="Discount factor")
    parser.add_argument("--gae-lambda", type=float, default=0.95, help="GAE lambda")
    parser.add_argument("--clip-epsilon", type=float, default=0.2, help="PPO clip epsilon")
    parser.add_argument("--entropy-coef", type=float, default=0.01, help="Entropy bonus coefficient")
    parser.add_argument("--opponent", choices=["self", "heuristic", "random"], default="self",
                        help="Opponent type")
    parser.add_argument("--output", type=str, default="ppo_weights.json", help="Output weights path")
    parser.add_argument("--save-every", type=int, default=25, help="Save checkpoint every N batches")
    parser.add_argument("--device", type=str, default="cpu", help="Device (cpu/cuda/mps)")
    args = parser.parse_args()

    config = PPOConfig(
        num_games_per_batch=args.num_games,
        num_batches=args.num_batches,
        lr=args.lr,
        ppo_epochs=args.ppo_epochs,
        minibatch_size=args.minibatch_size,
        gamma=args.gamma,
        gae_lambda=args.gae_lambda,
        clip_epsilon=args.clip_epsilon,
        entropy_coef=args.entropy_coef,
        opponent=args.opponent,
        output=args.output,
        save_every=args.save_every,
        device=args.device,
    )

    train(config)


if __name__ == "__main__":
    main()
