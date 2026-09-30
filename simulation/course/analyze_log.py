"""Validate a selected voltage-era firmware CSV without inventing missing fields.

The input must contain a CSV header and numeric data only. Copy the CSV region
from log dump, preserve the original serial output separately, and confirm t in s.
"""
import argparse
import csv
import hashlib
import json
import math
import statistics
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--csv', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    required = ['t', 'voltage', 'motor.rl', 'motor.rr', 'motor.fr', 'motor.fl']
    with args.csv.open(newline='', encoding='utf-8-sig') as f:
        reader = csv.DictReader(f)
        missing = sorted(set(required)-set(reader.fieldnames or []))
        if missing:
            parser.error('missing required measured fields: '+', '.join(missing))
        rows = []
        for number, row in enumerate(reader, 2):
            try:
                sample = {k: float(row[k]) for k in required}
            except (TypeError, ValueError, KeyError):
                parser.error(f'invalid numeric data at CSV line {number}')
            if not all(math.isfinite(v) for v in sample.values()):
                parser.error(f'non-finite value at CSV line {number}')
            if sample['voltage'] <= 0:
                parser.error(f'invalid voltage at CSV line {number}; do not treat missing voltage as zero')
            if any(not 0 <= sample[k] <= 1 for k in required[2:]):
                parser.error(f'motor command outside normalized [0,1] at CSV line {number}')
            if rows and sample['t'] <= rows[-1]['t']:
                parser.error(f'time is not strictly increasing at CSV line {number}')
            rows.append(sample)
    if len(rows) < 2:
        parser.error('at least two samples are required')
    dt = [b['t']-a['t'] for a,b in zip(rows, rows[1:])]
    def stats(values):
        return {'mean': statistics.mean(values), 'min': min(values), 'max': max(values),
                'population_std': statistics.pstdev(values)}
    report = {'status': 'data_checks_passed', 'source_sha256': hashlib.sha256(args.csv.read_bytes()).hexdigest(),
              'rows': len(rows), 'duration_s': rows[-1]['t']-rows[0]['t'], 'dt_s': stats(dt),
              'voltage_V': stats([r['voltage'] for r in rows]),
              'mean_four_motor_command': stats([sum(r[k] for k in required[2:])/4 for r in rows]),
              'time_gaps_over_2x_median': sum(d > 2*statistics.median(dt) for d in dt),
              'scope': 'window statistics only; not automatic hover detection or thrust identification'}
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output/'log-summary.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
