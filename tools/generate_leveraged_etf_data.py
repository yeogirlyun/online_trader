#!/usr/bin/env python3
"""
Generate Synthetic Leveraged ETF Data

This script generates synthetic data for leveraged ETFs based on underlying
index movements. Useful when historical leveraged ETF data is unavailable.

Leveraged ETFs:
- TQQQ: 3x QQQ (long Nasdaq)
- SQQQ: -3x QQQ (short Nasdaq)
- UPRO: 3x SPY (long S&P 500)
- SDS: -2x SPY (short S&P 500)
- UVXY: ~1.5x VIX (volatility, inverse SPY)
- SVIX: -1x VIX (short volatility, mimics SPY)

Usage:
    python3 tools/generate_leveraged_etf_data.py \
        --spy data/equities/SPY_RTH_NH.csv \
        --qqq data/equities/QQQ_RTH_NH.csv \
        --output data/equities/
"""

import pandas as pd
import numpy as np
import argparse
from pathlib import Path


def load_csv(filepath):
    """Load CSV with OHLCV data."""
    df = pd.read_csv(filepath)

    # Ensure timestamp column exists
    if 'timestamp' not in df.columns:
        raise ValueError(f"No 'timestamp' column in {filepath}")

    return df


def generate_leveraged_etf(base_df, leverage, start_price=100.0):
    """
    Generate synthetic leveraged ETF from base index.

    Args:
        base_df: DataFrame with OHLCV data
        leverage: Leverage ratio (e.g., 3.0 for 3x, -3.0 for -3x)
        start_price: Starting price for synthetic ETF

    Returns:
        DataFrame with synthetic leveraged ETF data
    """
    df = base_df.copy()

    # Calculate base returns
    df['base_return'] = df['close'].pct_change()

    # Apply leverage (intraday - no decay)
    df['lev_return'] = df['base_return'] * leverage

    # Generate price series
    df['lev_close'] = start_price * (1 + df['lev_return']).cumprod()

    # Generate OHLC from close (approximate)
    # For simplicity, scale base OHLC by same ratio as close
    price_ratio = df['lev_close'] / df['close']
    df['lev_open'] = df['open'] * price_ratio
    df['lev_high'] = df['high'] * price_ratio
    df['lev_low'] = df['low'] * price_ratio

    # Volume: inversely proportional to leverage (leveraged ETFs trade less)
    df['lev_volume'] = (df['volume'] / abs(leverage)).astype(int)

    # Create output DataFrame
    result = pd.DataFrame({
        'timestamp': df['timestamp'],
        'open': df['lev_open'],
        'high': df['lev_high'],
        'low': df['lev_low'],
        'close': df['lev_close'],
        'volume': df['lev_volume']
    })

    # Drop first row (NaN due to pct_change)
    result = result.dropna()

    return result


def generate_vix_proxy(spy_df, leverage, start_price=15.0):
    """
    Generate VIX-based ETF proxy from SPY volatility.

    VIX tends to move inverse to SPY. We approximate:
    - UVXY: 1.5x volatility (inverse SPY with amplification)
    - SVIX: -1x volatility (mimics SPY direction)

    Args:
        spy_df: DataFrame with SPY OHLCV data
        leverage: Leverage factor for volatility
        start_price: Starting price

    Returns:
        DataFrame with synthetic VIX ETF data
    """
    df = spy_df.copy()

    # Calculate SPY returns
    df['spy_return'] = df['close'].pct_change()

    # Calculate rolling volatility (20-bar)
    df['spy_vol'] = df['spy_return'].rolling(window=20).std()

    # VIX proxy: inverse SPY return + volatility amplification
    # When SPY drops, VIX rises (inverse)
    # When SPY is volatile, VIX rises (amplification)
    df['vix_return'] = -df['spy_return'] * leverage

    # Add volatility boost
    vol_mean = df['spy_vol'].mean()
    df['vol_boost'] = (df['spy_vol'] - vol_mean) / vol_mean * 0.5
    df['vix_return'] = df['vix_return'] + df['vol_boost'] * leverage

    # Generate price series
    df['vix_close'] = start_price * (1 + df['vix_return']).cumprod()

    # Generate OHLC (VIX ETFs are more volatile)
    volatility_mult = 1.2  # VIX ETFs have higher intrabar volatility
    df['vix_open'] = df['vix_close'] * (1 + np.random.normal(0, 0.01, len(df)))
    df['vix_high'] = df['vix_close'] * (1 + abs(np.random.normal(0, 0.02, len(df))) * volatility_mult)
    df['vix_low'] = df['vix_close'] * (1 - abs(np.random.normal(0, 0.02, len(df))) * volatility_mult)

    # Volume: VIX ETFs trade heavily
    df['vix_volume'] = (df['volume'] * 1.5).astype(int)

    # Create output DataFrame
    result = pd.DataFrame({
        'timestamp': df['timestamp'],
        'open': df['vix_open'],
        'high': df['vix_high'],
        'low': df['vix_low'],
        'close': df['vix_close'],
        'volume': df['vix_volume']
    })

    # Drop first rows (NaN due to rolling)
    result = result.dropna()

    return result


def save_csv(df, filepath):
    """Save DataFrame to CSV."""
    df.to_csv(filepath, index=False)
    print(f"  Saved: {filepath} ({len(df)} bars)")


def main():
    parser = argparse.ArgumentParser(description="Generate synthetic leveraged ETF data")
    parser.add_argument("--spy", required=True, help="Path to SPY CSV file")
    parser.add_argument("--qqq", required=True, help="Path to QQQ CSV file")
    parser.add_argument("--output", required=True, help="Output directory")
    parser.add_argument("--start-prices", nargs=6, type=float,
                       default=[50.0, 20.0, 80.0, 30.0, 15.0, 50.0],
                       help="Starting prices for TQQQ SQQQ UPRO SDS UVXY SVIX")

    args = parser.parse_args()

    # Load base data
    print("Loading base data...")
    spy_df = load_csv(args.spy)
    qqq_df = load_csv(args.qqq)
    print(f"  SPY: {len(spy_df)} bars")
    print(f"  QQQ: {len(qqq_df)} bars")

    # Create output directory
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    print("\nGenerating leveraged ETFs...")

    # TQQQ: 3x QQQ
    print("  TQQQ (3x QQQ)...")
    tqqq = generate_leveraged_etf(qqq_df, leverage=3.0, start_price=args.start_prices[0])
    save_csv(tqqq, output_dir / "TQQQ_RTH_NH.csv")

    # SQQQ: -3x QQQ
    print("  SQQQ (-3x QQQ)...")
    sqqq = generate_leveraged_etf(qqq_df, leverage=-3.0, start_price=args.start_prices[1])
    save_csv(sqqq, output_dir / "SQQQ_RTH_NH.csv")

    # UPRO: 3x SPY
    print("  UPRO (3x SPY)...")
    upro = generate_leveraged_etf(spy_df, leverage=3.0, start_price=args.start_prices[2])
    save_csv(upro, output_dir / "UPRO_RTH_NH.csv")

    # SDS: -2x SPY
    print("  SDS (-2x SPY)...")
    sds = generate_leveraged_etf(spy_df, leverage=-2.0, start_price=args.start_prices[3])
    save_csv(sds, output_dir / "SDS_RTH_NH.csv")

    # UVXY: 1.5x VIX proxy (inverse SPY + volatility)
    print("  UVXY (~1.5x VIX, inverse SPY + volatility)...")
    uvxy = generate_vix_proxy(spy_df, leverage=1.5, start_price=args.start_prices[4])
    save_csv(uvxy, output_dir / "UVXY_RTH_NH.csv")

    # SVIX: -1x VIX proxy (follows SPY)
    print("  SVIX (~-1x VIX, follows SPY)...")
    svix = generate_vix_proxy(spy_df, leverage=-1.0, start_price=args.start_prices[5])
    save_csv(svix, output_dir / "SVIX_RTH_NH.csv")

    print("\n" + "="*80)
    print("Synthetic ETF generation complete!")
    print("="*80)
    print("\nNOTE: This is synthetic data for testing purposes.")
    print("      For production trading, download real ETF data using:")
    print("      ./scripts/download_6_symbols.sh")
    print("\nGenerated files:")
    print(f"  {output_dir}/TQQQ_RTH_NH.csv")
    print(f"  {output_dir}/SQQQ_RTH_NH.csv")
    print(f"  {output_dir}/UPRO_RTH_NH.csv")
    print(f"  {output_dir}/SDS_RTH_NH.csv")
    print(f"  {output_dir}/UVXY_RTH_NH.csv")
    print(f"  {output_dir}/SVIX_RTH_NH.csv")


if __name__ == "__main__":
    main()
