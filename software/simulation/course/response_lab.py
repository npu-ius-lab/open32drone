"""Offline PD response comparison; illustrative unit-mass system, not firmware tuning."""
import argparse
import csv
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    dt = 0.01
    cases = [('low_damping', 4., 0.4, 0.), ('damped_pd', 4., 3.2, 0.), ('constant_disturbance', 4., 3.2, 0.4)]
    figure, axes = plt.subplots(2, 1, figsize=(9, 6), sharex=True, constrained_layout=True)
    report = {'model': 'unit-mass 1-D illustrative double integrator', 'dt_s': dt, 'target_m': 1., 'cases': {}}
    for name, kp, kd, disturbance in cases:
        x, v, rows = 0., 0., []
        for step in range(1200):
            u = max(-3., min(3., kp*(1-x)-kd*v))
            rows.append({'t': step*dt, 'position_m': x, 'velocity_m_s': v, 'command_m_s2': u})
            v += (u+disturbance)*dt
            x += v*dt
        with (args.output/f'{name}.csv').open('w', newline='') as f:
            w = csv.DictWriter(f, fieldnames=rows[0].keys()); w.writeheader(); w.writerows(rows)
        axes[0].plot([r['t'] for r in rows], [r['position_m'] for r in rows], label=name)
        axes[1].plot([r['t'] for r in rows], [r['command_m_s2'] for r in rows], label=name)
        report['cases'][name] = {'kp': kp, 'kd': kd, 'disturbance_m_s2': disturbance,
                                'overshoot_m': max(0.,max(r['position_m'] for r in rows)-1),
                                'tail_error_m': 1-sum(r['position_m'] for r in rows[-200:])/200,
                                'saturation_fraction': sum(abs(r['command_m_s2'])>=3 for r in rows)/len(rows)}
    axes[0].axhline(1., color='#888', linestyle='--', label='target')
    axes[0].set_ylabel('Position (m)'); axes[1].set_ylabel('Command (m/s²)'); axes[1].set_xlabel('Time (s)')
    for ax in axes: ax.grid(alpha=.25); ax.legend(fontsize=8)
    figure.savefig(args.output/'response.svg'); figure.savefig(args.output/'response.png', dpi=160); plt.close(figure)
    (args.output/'metrics.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
