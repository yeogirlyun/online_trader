#!/usr/bin/env python3
"""
Visualization Comparison Script
===============================

This script demonstrates the difference between the old primitive
visualize_tradebook.py and the new professional trading dashboard.

Usage:
    python compare_visualizations.py --tradebook your_trades.jsonl
"""

import argparse
import subprocess
import os
import sys
from datetime import datetime


def run_old_visualization(tradebook_path: str, output_path: str):
    """Run the old primitive visualization"""
    print("🔄 Running old primitive visualization...")
    try:
        cmd = [
            "python", "tools/visualize_tradebook.py",
            "--tradebook", tradebook_path,
            "--out", output_path,
            "--data", "data/equities/SPY_RTH_NH.csv"
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            print("✅ Old visualization completed")
            return True
        else:
            print(f"❌ Old visualization failed: {result.stderr}")
            return False
    except Exception as e:
        print(f"❌ Error running old visualization: {e}")
        return False


def run_new_visualization(tradebook_path: str, output_path: str):
    """Run the new professional visualization"""
    print("🚀 Running new professional visualization...")
    try:
        cmd = [
            "python", "scripts/professional_trading_dashboard.py",
            "--tradebook", tradebook_path,
            "--data", "data/equities/SPY_RTH_NH.csv",
            "--output", output_path
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            print("✅ New professional visualization completed")
            return True
        else:
            print(f"❌ New visualization failed: {result.stderr}")
            return False
    except Exception as e:
        print(f"❌ Error running new visualization: {e}")
        return False


def compare_file_sizes(old_path: str, new_path: str):
    """Compare file sizes of the generated visualizations"""
    if os.path.exists(old_path) and os.path.exists(new_path):
        old_size = os.path.getsize(old_path)
        new_size = os.path.getsize(new_path)
        
        print(f"\n📊 File Size Comparison:")
        print(f"   Old visualization: {old_size:,} bytes ({old_size/1024/1024:.1f} MB)")
        print(f"   New visualization: {new_size:,} bytes ({new_size/1024/1024:.1f} MB)")
        print(f"   Size difference: {new_size - old_size:,} bytes ({((new_size/old_size - 1) * 100):+.1f}%)")


def main():
    parser = argparse.ArgumentParser(description="Compare old vs new trading visualizations")
    parser.add_argument("--tradebook", required=True, help="Path to tradebook JSONL file")
    parser.add_argument("--compare", action="store_true", help="Run both visualizations for comparison")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.tradebook):
        print(f"❌ Tradebook not found: {args.tradebook}")
        return 1
    
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    if args.compare:
        print("🔄 Running comparison of old vs new visualizations...")
        
        # Run old visualization
        old_output = f"old_visualization_{timestamp}.html"
        old_success = run_old_visualization(args.tradebook, old_output)
        
        # Run new visualization
        new_output = f"new_visualization_{timestamp}.html"
        new_success = run_new_visualization(args.tradebook, new_output)
        
        if old_success and new_success:
            compare_file_sizes(old_output, new_output)
            print(f"\n🎉 Comparison complete!")
            print(f"📊 Open both files in your browser to compare:")
            print(f"   Old: {old_output}")
            print(f"   New: {new_output}")
        else:
            print("❌ Comparison failed - one or both visualizations failed")
            return 1
    else:
        # Just run the new professional visualization
        output = f"professional_dashboard_{timestamp}.html"
        success = run_new_visualization(args.tradebook, output)
        
        if success:
            print(f"\n🎉 Professional dashboard generated!")
            print(f"📊 Open {output} in your browser to view the dashboard")
        else:
            print("❌ Professional dashboard generation failed")
            return 1
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
