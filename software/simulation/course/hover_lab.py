"""A stationary six-DoF hover exercise using the existing residual controller.

CPU is supported for classroom use. No ROS, simulator GUI or hardware connection.
Training, export and deterministic held-out comparisons are separate records.
"""
import argparse
import csv
import hashlib
import json
import shutil
import sys
import time
from pathlib import Path

import torch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'rl_demo'))
from dynamics import ActorCritic, DroneEnv, MODEL


class HoverEnv(DroneEnv):
    def __init__(self, n, seed, device='cpu', wind_limit=0.8):
        self.wind_limit = wind_limit
        super().__init__(n, device=device, seed=seed, randomize=False)

    def reference(self):
        point = torch.zeros(self.n, 3, device=self.device)
        point[:, 2] = 1.5
        zeros = torch.zeros_like(point)
        return point, zeros, zeros, torch.zeros(self.n, device=self.device)

    def reset(self, ids):
        super().reset(ids)
        self.wind[ids] = 0
        # Constant horizontal disturbance per episode; its value is not observed.
        self.wind[ids, 0] = (2 * torch.rand(len(ids), device=self.device) - 1) * self.wind_limit


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def evaluate(net, label, device, seeds):
    results = []
    for seed in seeds:
        for wind in (0.0, 0.8, 1.5):
            env = HoverEnv(32, seed=seed, device=device)
            env.wind[:] = 0
            env.wind[:, 0] = wind
            alive = torch.ones(32, dtype=torch.bool, device=device)
            first_failure = torch.full((32,), 12.0, device=device)
            sums = torch.zeros(32, device=device)
            counts = torch.zeros(32, device=device)
            with torch.inference_mode():
                for step in range(600):
                    action = torch.zeros(32, 3, device=device) if net is None else net(env.observation())
                    _, _, _, info = env.step(action, reset=False)
                    finite = torch.isfinite(info['error'])
                    failed = info['died'] | ~finite
                    new_failure = alive & failed
                    first_failure[new_failure] = (step + 1) * env.dt
                    # Count only valid pre-failure data, excluding the first 2 s.
                    valid = alive & ~failed
                    if step >= 100:
                        sums[valid] += info['error'][valid].square()
                        counts[valid] += 1
                    alive &= ~failed
                    if not alive.any():
                        break
            eligible = counts > 0
            pooled = (sums[eligible].sum() / counts[eligible].sum()).sqrt().item() if eligible.any() else None
            complete = alive & eligible
            survivor = (sums[complete].sum() / counts[complete].sum()).sqrt().item() if complete.any() else None
            results.append({'controller': label, 'seed': seed, 'wind_x_m_s2': wind,
                            'trials': 32, 'duration_s': 12, 'warmup_excluded_s': 2,
                            'survived_trials': int(alive.sum()),
                            'rms_pre_failure_m': pooled, 'rms_complete_trials_m': survivor,
                            'first_failure_time_s': first_failure.cpu().tolist(),
                            'valid_samples': int(counts.sum())})
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--iterations', type=int, default=80)
    parser.add_argument('--envs', type=int, default=64)
    parser.add_argument('--seed', type=int, default=7)
    parser.add_argument('--device', choices=('cpu', 'cuda'), default='cpu')
    args = parser.parse_args()
    if args.iterations < 1 or args.envs < 8:
        parser.error('iterations >= 1 and envs >= 8 are required')
    if args.output.exists() and any(args.output.iterdir()):
        parser.error('output must be new or empty; preserve previous experiments')
    args.output.mkdir(parents=True, exist_ok=True)
    torch.set_num_threads(4)
    env = HoverEnv(args.envs, seed=args.seed, device=args.device)
    net = ActorCritic().to(args.device)
    net.log_std.data.fill_(-1.5)
    optimizer = torch.optim.Adam(net.parameters(), lr=3e-4)
    config = {'task': 'stationary hover at [0,0,1.5] m', 'algorithm': 'PPO residual acceleration',
              'device': args.device, 'torch': str(torch.__version__), 'seed': args.seed,
              'iterations': args.iterations, 'envs': args.envs, 'rollout_steps': 64,
              'epochs': 4, 'minibatches': 4, 'gamma': 0.99, 'gae_lambda': 0.95,
              'ppo_clip': 0.2, 'learning_rate': 3e-4, 'entropy_coefficient': 0.001,
              'initial_log_std': -1.5,
              'model': MODEL, 'observation_dim': 35, 'action_dim': 3,
              'training_wind_x_range_m_s2': [-0.8, 0.8],
              'randomization': 'constant x wind per episode; other physical parameters nominal',
              'timeout_bootstrap': True, 'sensor_feedback': 'simulation state, not estimated sensors',
              'hardware_connection': False, 'evaluation_seeds': [1001, 1002, 1003],
              'evaluation_case_1_5_m_s2': 'outside training wind range',
              'selection': 'fixed iteration budget; no test-based checkpoint selection'}
    snapshot = args.output / 'source'
    (snapshot / 'course').mkdir(parents=True)
    (snapshot / 'rl_demo').mkdir()
    shutil.copy2(__file__, snapshot / 'course/hover_lab.py')
    for name in ('dynamics.py', 'model.json'):
        shutil.copy2(Path(__file__).resolve().parents[1] / 'rl_demo' / name, snapshot / 'rl_demo' / name)
    (args.output / 'config.json').write_text(json.dumps(config, indent=2) + '\n')
    def save(name, iteration):
        torch.save({'model': net.state_dict(), 'iteration': iteration, 'config': config}, args.output / name)
    save('policy_initial.pt', 0)
    obs = env.observation()
    start = time.monotonic()
    with (args.output / 'training.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=['iteration', 'steps', 'elapsed_s', 'reward_mean', 'error_mean_m', 'failures', 'policy_loss', 'value_loss'])
        writer.writeheader()
        for iteration in range(1, args.iterations+1):
            observations, actions, logps, values, rewards, dones = [], [], [], [], [], []
            errors, failures, raw_rewards = [], [], []
            with torch.no_grad():
                for _ in range(64):
                    dist = net.distribution(obs)
                    action = dist.sample()
                    observations.append(obs)
                    actions.append(action)
                    logps.append(dist.log_prob(action).sum(-1))
                    values.append(net.critic(obs).squeeze(-1))
                    terminal_obs, reward, done, info = env.step(action, reset=False)
                    if not torch.isfinite(terminal_obs).all() or not torch.isfinite(reward).all():
                        raise RuntimeError('non-finite dynamics; do not export a success result')
                    truncated = done & ~info['died']
                    raw_rewards.append(reward)
                    reward = reward + 0.99 * net.critic(terminal_obs).squeeze(-1) * truncated
                    rewards.append(reward)
                    dones.append(done.float())
                    errors.append(info['error'])
                    failures.append(info['died'])
                    env.reset(torch.where(done)[0])
                    obs = env.observation()
                last = net.critic(obs).squeeze(-1)
                advantage = torch.zeros(64, args.envs, device=args.device)
                gae = torch.zeros(args.envs, device=args.device)
                for t in reversed(range(64)):
                    next_value = last if t == 63 else values[t+1]
                    mask = 1-dones[t]
                    delta = rewards[t] + 0.99 * next_value * mask - values[t]
                    gae = delta + 0.99 * 0.95 * mask * gae
                    advantage[t] = gae
                returns = (advantage + torch.stack(values)).flatten()
                adv = advantage.flatten()
                adv = (adv-adv.mean())/(adv.std()+1e-8)
                batch_obs = torch.stack(observations).flatten(0, 1)
                batch_action = torch.stack(actions).flatten(0, 1)
                old_logp = torch.stack(logps).flatten()
            for _ in range(4):
                for ids in torch.randperm(len(adv), device=args.device).chunk(4):
                    dist = net.distribution(batch_obs[ids])
                    ratio = (dist.log_prob(batch_action[ids]).sum(-1)-old_logp[ids]).exp()
                    policy_loss = -torch.minimum(ratio*adv[ids], ratio.clamp(0.8,1.2)*adv[ids]).mean()
                    value_loss = 0.5*(net.critic(batch_obs[ids]).squeeze(-1)-returns[ids]).square().mean()
                    loss = policy_loss+value_loss-0.001*dist.entropy().sum(-1).mean()
                    if not torch.isfinite(loss):
                        raise RuntimeError('non-finite PPO loss')
                    optimizer.zero_grad()
                    loss.backward()
                    torch.nn.utils.clip_grad_norm_(list(net.actor.parameters())+[net.log_std], 1.0)
                    torch.nn.utils.clip_grad_norm_(net.critic.parameters(), 1.0)
                    optimizer.step()
            row = {'iteration': iteration, 'steps': iteration*64*args.envs,
                   'elapsed_s': round(time.monotonic()-start, 3),
                   'reward_mean': torch.stack(raw_rewards).mean().item(),
                   'error_mean_m': torch.stack(errors).mean().item(),
                   'failures': int(torch.stack(failures).sum()),
                   'policy_loss': policy_loss.item(), 'value_loss': value_loss.item()}
            writer.writerow(row)
            stream.flush()
            if iteration % 10 == 0 or iteration == 1:
                print(json.dumps(row), flush=True)
    save('policy_final.pt', args.iterations)
    net.eval()
    example = torch.randn(32, 35, device=args.device).clamp(-5,5)
    actor = torch.jit.trace(net, example)
    actor.save(str(args.output / 'actor.pt'))
    loaded = torch.jit.load(str(args.output / 'actor.pt'), map_location=args.device)
    with torch.inference_mode():
        export_error = (loaded(example)-net(example)).abs().max().item()
    assert export_error < 1e-6
    cases = evaluate(None, 'baseline_pd', args.device, config['evaluation_seeds'])
    cases += evaluate(net, 'trained_ppo_residual', args.device, config['evaluation_seeds'])
    report = {'status': 'completed', 'backend': 'PyTorch CPU/CUDA six-DoF surrogate',
              'configuration': config, 'cases': cases, 'export_max_abs_error': export_error,
              'elapsed_s': time.monotonic()-start, 'improvement_is_not_a_completion_requirement': True,
              'policy_sha256': sha(args.output / 'policy_final.pt'),
              'scope': 'teaching workflow; convergence, Isaac and real-flight acceptance are separate'}
    (args.output / 'evaluation.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    files = sorted(p for p in args.output.rglob('*') if p.is_file() and p.name != 'SHA256SUMS')
    (args.output / 'SHA256SUMS').write_text(''.join(f'{sha(p)}  {p.relative_to(args.output)}\n' for p in files))
    print(json.dumps({'status': 'completed', 'export_error': export_error, 'evaluation_rows': len(cases)}), flush=True)


if __name__ == '__main__':
    main()
