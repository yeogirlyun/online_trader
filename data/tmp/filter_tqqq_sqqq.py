#!/usr/bin/env python3
"""Filter TQQQ and SQQQ to match SPY warmup time range."""

import csv
from datetime import datetime

# Read SPY to get date range
spy_file = "SPY_20blocks_for_warmup.csv"
with open(spy_file) as f:
    reader = csv.DictReader(f)
    rows = list(reader)
    start_time = rows[0]['ts_utc']
    end_time = rows[-1]['ts_utc']

print(f"SPY range: {start_time} to {end_time}")

# Filter TQQQ and SQQQ
for symbol in ['TQQQ', 'SQQQ']:
    input_file = f"../equities/{symbol}_RTH_NH.csv"
    output_file = f"rotation_warmup/{symbol}_RTH_NH.csv"

    with open(input_file) as f_in:
        reader = csv.DictReader(f_in)
        rows = []

        for row in reader:
            ts = row['ts_utc']
            # Convert to comparable format
            if ts >= start_time and ts <= end_time:
                rows.append(row)

        print(f"{symbol}: Filtered {len(rows)} bars (from {rows[0]['ts_utc']} to {rows[-1]['ts_utc']})")

        # Write output
        with open(output_file, 'w') as f_out:
            if rows:
                writer = csv.DictWriter(f_out, fieldnames=rows[0].keys())
                writer.writeheader()
                writer.writerows(rows)
                print(f"✅ Wrote {output_file}")
