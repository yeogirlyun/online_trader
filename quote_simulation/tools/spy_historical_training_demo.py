#!/usr/bin/env python3
"""
SPY Historical Data Training and Quote Generation Demo
=====================================================

This script demonstrates how MarS can "train" on historical SPY data
and generate realistic quote simulations that continue from the historical patterns.

Key Features:
- Loads historical SPY data (RTH/NH format)
- Uses MarS AI to learn market patterns
- Generates realistic continuation quotes
- Exports in multiple formats for analysis
"""

import sys
import os
import json
import pandas as pd
import numpy as np
from pathlib import Path
from datetime import datetime, timedelta
from typing import List, Dict, Any
import argparse

# Add MarS to Python path
mars_path = Path(__file__).parent / "MarS"
sys.path.insert(0, str(mars_path))

try:
    from mars_bridge import MarsDataGenerator
    MARS_AVAILABLE = True
except ImportError:
    MARS_AVAILABLE = False
    print("Warning: MarS not available")

def load_spy_historical_data(data_file: str) -> pd.DataFrame:
    """Load SPY historical data from CSV file."""
    print(f"📊 Loading SPY historical data from {data_file}")
    
    if not os.path.exists(data_file):
        raise FileNotFoundError(f"SPY data file not found: {data_file}")
    
    # Load CSV data
    df = pd.read_csv(data_file)
    
    # Convert timestamp columns
    if 'ts_utc' in df.columns:
        df['ts_utc'] = pd.to_datetime(df['ts_utc'])
    if 'ts_nyt_epoch' in df.columns:
        df['ts_nyt_epoch'] = pd.to_datetime(df['ts_nyt_epoch'], unit='s')
    
    print(f"✅ Loaded {len(df)} SPY bars")
    print(f"📅 Date range: {df['ts_utc'].min()} to {df['ts_utc'].max()}")
    print(f"💰 Price range: ${df['low'].min():.2f} - ${df['high'].max():.2f}")
    
    return df

def analyze_spy_patterns(df: pd.DataFrame) -> Dict[str, Any]:
    """Analyze SPY historical patterns for MarS training."""
    print("\n🔍 Analyzing SPY historical patterns...")
    
    # Calculate returns
    df['returns'] = df['close'].pct_change()
    df['log_returns'] = np.log(df['close'] / df['close'].shift(1))
    
    # Calculate volatility (rolling 20-period)
    df['volatility'] = df['returns'].rolling(window=20).std()
    
    # Calculate volume patterns
    df['volume_ma'] = df['volume'].rolling(window=20).mean()
    df['volume_ratio'] = df['volume'] / df['volume_ma']
    
    # Market regime analysis
    high_vol_threshold = df['volatility'].quantile(0.8)
    low_vol_threshold = df['volatility'].quantile(0.2)
    
    df['regime'] = 'normal'
    df.loc[df['volatility'] > high_vol_threshold, 'regime'] = 'volatile'
    df.loc[df['volatility'] < low_vol_threshold, 'regime'] = 'low_vol'
    
    # Pattern statistics
    patterns = {
        'total_bars': len(df),
        'avg_return': df['returns'].mean(),
        'volatility': df['returns'].std(),
        'skewness': df['returns'].skew(),
        'kurtosis': df['returns'].kurtosis(),
        'avg_volume': df['volume'].mean(),
        'volume_volatility': df['volume'].std(),
        'regime_distribution': df['regime'].value_counts().to_dict(),
        'price_range': {
            'min': df['low'].min(),
            'max': df['high'].max(),
            'current': df['close'].iloc[-1]
        }
    }
    
    print(f"📈 Average return: {patterns['avg_return']:.4f} ({patterns['avg_return']*100:.2f}%)")
    print(f"📊 Volatility: {patterns['volatility']:.4f} ({patterns['volatility']*100:.2f}%)")
    print(f"📦 Average volume: {patterns['avg_volume']:,.0f}")
    print(f"🎯 Current price: ${patterns['price_range']['current']:.2f}")
    print(f"📊 Regime distribution: {patterns['regime_distribution']}")
    
    return patterns

def generate_spy_continuation_quotes(historical_data_file: str,
                                   continuation_minutes: int = 60,
                                   num_simulations: int = 5,
                                   use_mars_ai: bool = True) -> List[Dict[str, Any]]:
    """Generate SPY quote continuation using MarS trained on historical data."""
    
    if not MARS_AVAILABLE:
        print("❌ MarS not available - cannot generate AI-powered continuation")
        return []
    
    print(f"\n🤖 Generating SPY continuation quotes using MarS AI")
    print(f"📊 Historical data: {historical_data_file}")
    print(f"⏱️  Continuation: {continuation_minutes} minutes")
    print(f"🎲 Simulations: {num_simulations}")
    print(f"🧠 MarS AI: {'Enabled' if use_mars_ai else 'Disabled'}")
    
    # Create MarS data generator
    mars_generator = MarsDataGenerator(symbol="SPY", base_price=450.0)
    
    # Generate continuation data using historical context
    continuation_data = mars_generator.generate_market_data_with_historical_context(
        historical_data_file=historical_data_file,
        continuation_minutes=continuation_minutes,
        bar_interval_seconds=60,  # 1-minute bars
        num_simulations=num_simulations,
        use_mars_ai=use_mars_ai
    )
    
    print(f"✅ Generated {len(continuation_data)} continuation bars")
    
    return continuation_data

def export_continuation_data(data: List[Dict[str, Any]], 
                           output_file: str,
                           format: str = "json") -> None:
    """Export continuation data to file."""
    
    if not data:
        print("⚠️ No data to export")
        return
    
    if format == "json":
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2, default=str)
        print(f"✅ Exported {len(data)} bars to {output_file}")
    
    elif format == "csv":
        df = pd.DataFrame(data)
        df.to_csv(output_file, index=False)
        print(f"✅ Exported {len(data)} bars to {output_file}")
    
    else:
        print(f"❌ Unknown format: {format}")

def create_spy_quote_simulation_demo(historical_data_file: str,
                                   continuation_minutes: int = 60,
                                   num_simulations: int = 5,
                                   output_dir: str = "spy_mars_demo") -> None:
    """Create a comprehensive SPY quote simulation demo."""
    
    print("🚀 SPY Historical Data Training and Quote Generation Demo")
    print("=" * 60)
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Step 1: Load and analyze historical data
    try:
        df = load_spy_historical_data(historical_data_file)
        patterns = analyze_spy_patterns(df)
        
        # Save analysis
        with open(f"{output_dir}/spy_patterns_analysis.json", 'w') as f:
            json.dump(patterns, f, indent=2, default=str)
        
    except Exception as e:
        print(f"❌ Failed to load historical data: {e}")
        return
    
    # Step 2: Generate continuation quotes using MarS
    try:
        continuation_data = generate_spy_continuation_quotes(
            historical_data_file=historical_data_file,
            continuation_minutes=continuation_minutes,
            num_simulations=num_simulations,
            use_mars_ai=True
        )
        
        if continuation_data:
            # Export continuation data
            export_continuation_data(continuation_data, 
                                   f"{output_dir}/spy_mars_continuation.json", 
                                   "json")
            export_continuation_data(continuation_data, 
                                   f"{output_dir}/spy_mars_continuation.csv", 
                                   "csv")
            
            # Analyze continuation patterns
            analyze_continuation_patterns(continuation_data, patterns, output_dir)
        
    except Exception as e:
        print(f"❌ Failed to generate continuation: {e}")
        return
    
    # Step 3: Create comparison report
    create_comparison_report(historical_data_file, continuation_data, output_dir)
    
    print(f"\n🎉 Demo completed! Results saved to {output_dir}/")
    print(f"📁 Files created:")
    print(f"   - {output_dir}/spy_patterns_analysis.json")
    print(f"   - {output_dir}/spy_mars_continuation.json")
    print(f"   - {output_dir}/spy_mars_continuation.csv")
    print(f"   - {output_dir}/comparison_report.json")

def analyze_continuation_patterns(continuation_data: List[Dict[str, Any]], 
                               historical_patterns: Dict[str, Any],
                               output_dir: str) -> None:
    """Analyze patterns in the continuation data."""
    
    print(f"\n🔍 Analyzing continuation patterns...")
    
    if not continuation_data:
        return
    
    # Convert to DataFrame for analysis
    df_cont = pd.DataFrame(continuation_data)
    
    # Calculate continuation statistics
    if 'close' in df_cont.columns and 'open' in df_cont.columns:
        df_cont['returns'] = df_cont['close'].pct_change()
        
        cont_stats = {
            'total_bars': len(df_cont),
            'avg_return': df_cont['returns'].mean(),
            'volatility': df_cont['returns'].std(),
            'skewness': df_cont['returns'].skew(),
            'kurtosis': df_cont['returns'].kurtosis(),
            'price_range': {
                'min': df_cont['low'].min() if 'low' in df_cont.columns else None,
                'max': df_cont['high'].max() if 'high' in df_cont.columns else None,
                'start': df_cont['open'].iloc[0] if 'open' in df_cont.columns else None,
                'end': df_cont['close'].iloc[-1] if 'close' in df_cont.columns else None
            }
        }
        
        # Compare with historical patterns
        comparison = {
            'historical': historical_patterns,
            'continuation': cont_stats,
            'similarity_score': calculate_similarity_score(historical_patterns, cont_stats)
        }
        
        # Save analysis
        with open(f"{output_dir}/continuation_analysis.json", 'w') as f:
            json.dump(comparison, f, indent=2, default=str)
        
        print(f"📊 Continuation statistics:")
        print(f"   Average return: {cont_stats['avg_return']:.4f} ({cont_stats['avg_return']*100:.2f}%)")
        print(f"   Volatility: {cont_stats['volatility']:.4f} ({cont_stats['volatility']*100:.2f}%)")
        print(f"   Price change: ${cont_stats['price_range']['start']:.2f} → ${cont_stats['price_range']['end']:.2f}")
        print(f"   Similarity score: {comparison['similarity_score']:.2f}/10")

def calculate_similarity_score(historical: Dict[str, Any], continuation: Dict[str, Any]) -> float:
    """Calculate similarity score between historical and continuation patterns."""
    
    score = 0.0
    max_score = 10.0
    
    # Compare return patterns
    if 'avg_return' in historical and 'avg_return' in continuation:
        ret_diff = abs(historical['avg_return'] - continuation['avg_return'])
        score += max(0, 2.0 - ret_diff * 1000)  # Up to 2 points
    
    # Compare volatility patterns
    if 'volatility' in historical and 'volatility' in continuation:
        vol_diff = abs(historical['volatility'] - continuation['volatility'])
        score += max(0, 2.0 - vol_diff * 1000)  # Up to 2 points
    
    # Compare skewness patterns
    if 'skewness' in historical and 'skewness' in continuation:
        skew_diff = abs(historical['skewness'] - continuation['skewness'])
        score += max(0, 2.0 - skew_diff)  # Up to 2 points
    
    # Compare kurtosis patterns
    if 'kurtosis' in historical and 'kurtosis' in continuation:
        kurt_diff = abs(historical['kurtosis'] - continuation['kurtosis'])
        score += max(0, 2.0 - kurt_diff / 10)  # Up to 2 points
    
    # Compare volume patterns
    if 'avg_volume' in historical and 'avg_volume' in continuation:
        vol_ratio = continuation['avg_volume'] / historical['avg_volume']
        score += max(0, 2.0 - abs(vol_ratio - 1.0) * 2)  # Up to 2 points
    
    return min(score, max_score)

def create_comparison_report(historical_file: str, 
                           continuation_data: List[Dict[str, Any]], 
                           output_dir: str) -> None:
    """Create a comprehensive comparison report."""
    
    print(f"\n📊 Creating comparison report...")
    
    report = {
        'demo_info': {
            'timestamp': datetime.now().isoformat(),
            'historical_data_file': historical_file,
            'continuation_bars': len(continuation_data),
            'mars_ai_enabled': MARS_AVAILABLE
        },
        'summary': {
            'historical_data_loaded': os.path.exists(historical_file),
            'continuation_generated': len(continuation_data) > 0,
            'mars_available': MARS_AVAILABLE,
            'demo_successful': os.path.exists(historical_file) and len(continuation_data) > 0
        }
    }
    
    # Save report
    with open(f"{output_dir}/comparison_report.json", 'w') as f:
        json.dump(report, f, indent=2, default=str)
    
    print(f"✅ Comparison report created")

def main():
    """Main function for SPY historical data training demo."""
    
    parser = argparse.ArgumentParser(description="SPY Historical Data Training and Quote Generation Demo")
    parser.add_argument("--data", default="data/equities/SPY_RTH_NH.csv", 
                       help="Path to SPY historical data CSV file")
    parser.add_argument("--continuation", type=int, default=60, 
                       help="Minutes to continue after historical data")
    parser.add_argument("--simulations", type=int, default=5, 
                       help="Number of simulations to run")
    parser.add_argument("--output", default="spy_mars_demo", 
                       help="Output directory for results")
    parser.add_argument("--no-mars", action="store_true", 
                       help="Disable MarS AI (use basic simulation)")
    
    args = parser.parse_args()
    
    # Check if data file exists
    if not os.path.exists(args.data):
        print(f"❌ SPY data file not found: {args.data}")
        print(f"💡 Make sure you've downloaded SPY data using the data downloader")
        return 1
    
    # Create demo
    create_spy_quote_simulation_demo(
        historical_data_file=args.data,
        continuation_minutes=args.continuation,
        num_simulations=args.simulations,
        output_dir=args.output
    )
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
