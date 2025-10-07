#!/usr/bin/env python3
"""
SPY Historical Data Training Demo (Simplified)
==============================================

This script demonstrates how to use MarS with historical SPY data
to generate realistic quote simulations that learn from historical patterns.

Features:
- Loads historical SPY data
- Uses MarS AI to learn market patterns
- Generates realistic continuation quotes
- Works with available MarS components
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

def load_and_analyze_spy_data(data_file: str) -> Dict[str, Any]:
    """Load and analyze SPY historical data."""
    print(f"📊 Loading SPY historical data from {data_file}")
    
    if not os.path.exists(data_file):
        raise FileNotFoundError(f"SPY data file not found: {data_file}")
    
    # Load CSV data
    df = pd.read_csv(data_file)
    
    # Convert timestamp columns
    if 'ts_utc' in df.columns:
        df['ts_utc'] = pd.to_datetime(df['ts_utc'])
    
    print(f"✅ Loaded {len(df)} SPY bars")
    print(f"📅 Date range: {df['ts_utc'].min()} to {df['ts_utc'].max()}")
    print(f"💰 Price range: ${df['low'].min():.2f} - ${df['high'].max():.2f}")
    
    # Calculate patterns
    df['returns'] = df['close'].pct_change()
    df['volatility'] = df['returns'].rolling(window=20).std()
    
    patterns = {
        'total_bars': len(df),
        'avg_return': df['returns'].mean(),
        'volatility': df['returns'].std(),
        'skewness': df['returns'].skew(),
        'kurtosis': df['returns'].kurtosis(),
        'avg_volume': df['volume'].mean(),
        'current_price': df['close'].iloc[-1],
        'price_range': {
            'min': df['low'].min(),
            'max': df['high'].max()
        }
    }
    
    print(f"📈 Average return: {patterns['avg_return']:.4f} ({patterns['avg_return']*100:.2f}%)")
    print(f"📊 Volatility: {patterns['volatility']:.4f} ({patterns['volatility']*100:.2f}%)")
    print(f"📦 Average volume: {patterns['avg_volume']:,.0f}")
    print(f"🎯 Current price: ${patterns['current_price']:.2f}")
    
    return patterns

def generate_spy_quotes_with_mars(historical_patterns: Dict[str, Any],
                                continuation_minutes: int = 60,
                                num_simulations: int = 5,
                                market_regime: str = "normal") -> List[Dict[str, Any]]:
    """Generate SPY quotes using MarS trained on historical patterns."""
    
    if not MARS_AVAILABLE:
        print("❌ MarS not available - using basic simulation")
        return generate_basic_spy_quotes(historical_patterns, continuation_minutes, num_simulations)
    
    print(f"\n🤖 Generating SPY quotes using MarS AI")
    print(f"📊 Market regime: {market_regime}")
    print(f"⏱️  Continuation: {continuation_minutes} minutes")
    print(f"🎲 Simulations: {num_simulations}")
    
    # Create MarS data generator with SPY-specific parameters
    base_price = historical_patterns['current_price']
    mars_generator = MarsDataGenerator(symbol="SPY", base_price=base_price)
    
    # Generate market data using MarS
    mars_data = mars_generator.generate_market_data(
        duration_minutes=continuation_minutes,
        bar_interval_seconds=60,  # 1-minute bars
        num_simulations=num_simulations,
        market_regime=market_regime
    )
    
    print(f"✅ Generated {len(mars_data)} SPY bars using MarS")
    
    return mars_data

def generate_basic_spy_quotes(historical_patterns: Dict[str, Any],
                             continuation_minutes: int,
                             num_simulations: int) -> List[Dict[str, Any]]:
    """Generate basic SPY quotes based on historical patterns."""
    
    print(f"\n📊 Generating basic SPY quotes based on historical patterns")
    
    quotes = []
    base_price = historical_patterns['current_price']
    avg_return = historical_patterns['avg_return']
    volatility = historical_patterns['volatility']
    avg_volume = historical_patterns['avg_volume']
    
    for sim_idx in range(num_simulations):
        print(f"🔄 Simulation {sim_idx + 1}/{num_simulations}")
        
        current_price = base_price
        current_time = datetime.now()
        
        for minute in range(continuation_minutes):
            # Generate price movement based on historical patterns
            price_change = np.random.normal(avg_return, volatility)
            current_price *= (1 + price_change)
            
            # Generate volume based on historical patterns
            volume = int(np.random.normal(avg_volume, avg_volume * 0.3))
            volume = max(1000, volume)  # Minimum volume
            
            # Generate OHLC
            open_price = current_price
            high_price = current_price * (1 + abs(np.random.normal(0, volatility * 0.5)))
            low_price = current_price * (1 - abs(np.random.normal(0, volatility * 0.5)))
            close_price = current_price
            
            quote = {
                'timestamp': int((current_time + timedelta(minutes=minute)).timestamp()),
                'symbol': 'SPY',
                'open': round(open_price, 2),
                'high': round(high_price, 2),
                'low': round(low_price, 2),
                'close': round(close_price, 2),
                'volume': volume,
                'simulation': sim_idx,
                'minute': minute,
                'source': 'basic_simulation'
            }
            
            quotes.append(quote)
    
    print(f"✅ Generated {len(quotes)} basic SPY quotes")
    return quotes

def analyze_generated_quotes(quotes: List[Dict[str, Any]], 
                           historical_patterns: Dict[str, Any]) -> Dict[str, Any]:
    """Analyze the generated quotes and compare with historical patterns."""
    
    if not quotes:
        return {}
    
    print(f"\n🔍 Analyzing generated quotes...")
    
    # Convert to DataFrame
    df = pd.DataFrame(quotes)
    
    # Calculate statistics
    df['returns'] = df['close'].pct_change()
    
    generated_stats = {
        'total_bars': len(df),
        'avg_return': df['returns'].mean(),
        'volatility': df['returns'].std(),
        'skewness': df['returns'].skew(),
        'kurtosis': df['returns'].kurtosis(),
        'avg_volume': df['volume'].mean(),
        'price_range': {
            'min': df['low'].min(),
            'max': df['high'].max(),
            'start': df['open'].iloc[0],
            'end': df['close'].iloc[-1]
        }
    }
    
    # Calculate similarity score
    similarity_score = calculate_similarity_score(historical_patterns, generated_stats)
    
    print(f"📊 Generated quote statistics:")
    print(f"   Average return: {generated_stats['avg_return']:.4f} ({generated_stats['avg_return']*100:.2f}%)")
    print(f"   Volatility: {generated_stats['volatility']:.4f} ({generated_stats['volatility']*100:.2f}%)")
    print(f"   Price change: ${generated_stats['price_range']['start']:.2f} → ${generated_stats['price_range']['end']:.2f}")
    print(f"   Similarity score: {similarity_score:.2f}/10")
    
    return {
        'historical': historical_patterns,
        'generated': generated_stats,
        'similarity_score': similarity_score
    }

def calculate_similarity_score(historical: Dict[str, Any], generated: Dict[str, Any]) -> float:
    """Calculate similarity score between historical and generated patterns."""
    
    score = 0.0
    max_score = 10.0
    
    # Compare return patterns
    if 'avg_return' in historical and 'avg_return' in generated:
        ret_diff = abs(historical['avg_return'] - generated['avg_return'])
        score += max(0, 2.0 - ret_diff * 1000)  # Up to 2 points
    
    # Compare volatility patterns
    if 'volatility' in historical and 'volatility' in generated:
        vol_diff = abs(historical['volatility'] - generated['volatility'])
        score += max(0, 2.0 - vol_diff * 1000)  # Up to 2 points
    
    # Compare skewness patterns
    if 'skewness' in historical and 'skewness' in generated:
        skew_diff = abs(historical['skewness'] - generated['skewness'])
        score += max(0, 2.0 - skew_diff)  # Up to 2 points
    
    # Compare kurtosis patterns
    if 'kurtosis' in historical and 'kurtosis' in generated:
        kurt_diff = abs(historical['kurtosis'] - generated['kurtosis'])
        score += max(0, 2.0 - kurt_diff / 10)  # Up to 2 points
    
    # Compare volume patterns
    if 'avg_volume' in historical and 'avg_volume' in generated:
        vol_ratio = generated['avg_volume'] / historical['avg_volume']
        score += max(0, 2.0 - abs(vol_ratio - 1.0) * 2)  # Up to 2 points
    
    return min(score, max_score)

def export_results(quotes: List[Dict[str, Any]], 
                  analysis: Dict[str, Any], 
                  output_dir: str) -> None:
    """Export results to files."""
    
    os.makedirs(output_dir, exist_ok=True)
    
    # Export quotes
    if quotes:
        with open(f"{output_dir}/spy_generated_quotes.json", 'w') as f:
            json.dump(quotes, f, indent=2, default=str)
        
        df = pd.DataFrame(quotes)
        df.to_csv(f"{output_dir}/spy_generated_quotes.csv", index=False)
        
        print(f"✅ Exported {len(quotes)} quotes to {output_dir}/")
    
    # Export analysis
    if analysis:
        with open(f"{output_dir}/spy_analysis.json", 'w') as f:
            json.dump(analysis, f, indent=2, default=str)
        
        print(f"✅ Exported analysis to {output_dir}/spy_analysis.json")

def main():
    """Main function for SPY historical data training demo."""
    
    parser = argparse.ArgumentParser(description="SPY Historical Data Training Demo")
    parser.add_argument("--data", default="data/equities/SPY_RTH_NH.csv", 
                       help="Path to SPY historical data CSV file")
    parser.add_argument("--continuation", type=int, default=60, 
                       help="Minutes to continue after historical data")
    parser.add_argument("--simulations", type=int, default=5, 
                       help="Number of simulations to run")
    parser.add_argument("--regime", default="normal", 
                       choices=["normal", "volatile", "trending", "bear", "bull", "sideways"],
                       help="Market regime for simulation")
    parser.add_argument("--output", default="spy_training_demo", 
                       help="Output directory for results")
    
    args = parser.parse_args()
    
    print("🚀 SPY Historical Data Training Demo")
    print("=" * 50)
    
    # Check if data file exists
    if not os.path.exists(args.data):
        print(f"❌ SPY data file not found: {args.data}")
        print(f"💡 Make sure you've downloaded SPY data using the data downloader")
        return 1
    
    try:
        # Step 1: Load and analyze historical data
        historical_patterns = load_and_analyze_spy_data(args.data)
        
        # Step 2: Generate quotes using MarS
        quotes = generate_spy_quotes_with_mars(
            historical_patterns=historical_patterns,
            continuation_minutes=args.continuation,
            num_simulations=args.simulations,
            market_regime=args.regime
        )
        
        # Step 3: Analyze generated quotes
        analysis = analyze_generated_quotes(quotes, historical_patterns)
        
        # Step 4: Export results
        export_results(quotes, analysis, args.output)
        
        print(f"\n🎉 Demo completed! Results saved to {args.output}/")
        print(f"📁 Files created:")
        print(f"   - {args.output}/spy_generated_quotes.json")
        print(f"   - {args.output}/spy_generated_quotes.csv")
        print(f"   - {args.output}/spy_analysis.json")
        
        return 0
        
    except Exception as e:
        print(f"❌ Demo failed: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
