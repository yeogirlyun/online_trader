#!/usr/bin/env python3
"""
Screenshot Dashboard HTML
=========================

Takes a screenshot of the existing professional trading dashboard HTML file.
Uses Playwright to render the HTML and capture a full-page screenshot.

Requirements:
    pip install playwright
    playwright install chromium

Usage:
    python3 screenshot_dashboard.py \
        --dashboard data/dashboards/session_20251009_163724.html \
        --output /tmp/dashboard_screenshot.png \
        --width 1600 \
        --height 2400
"""

import argparse
import os
import sys
from pathlib import Path

def screenshot_with_playwright(html_path, output_path, width=1600, height=2400):
    """Take screenshot using Playwright (headless browser)"""
    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        print("❌ Playwright not installed")
        print("   Install with: pip install playwright && playwright install chromium")
        return False

    try:
        with sync_playwright() as p:
            # Launch browser
            browser = p.chromium.launch(headless=True)
            page = browser.new_page(viewport={'width': width, 'height': height})

            # Load HTML file
            html_url = f"file://{os.path.abspath(html_path)}"
            page.goto(html_url)

            # Wait for Plotly charts to render
            page.wait_for_timeout(2000)  # 2 seconds for charts to load

            # Take full page screenshot
            page.screenshot(path=output_path, full_page=True)

            browser.close()
            return True
    except Exception as e:
        print(f"❌ Screenshot failed: {e}")
        return False


def screenshot_with_selenium(html_path, output_path, width=1600, height=2400):
    """Take screenshot using Selenium (fallback method)"""
    try:
        from selenium import webdriver
        from selenium.webdriver.chrome.options import Options
        from selenium.webdriver.chrome.service import Service
    except ImportError:
        print("❌ Selenium not installed")
        print("   Install with: pip install selenium")
        return False

    try:
        # Configure Chrome options
        chrome_options = Options()
        chrome_options.add_argument('--headless')
        chrome_options.add_argument(f'--window-size={width},{height}')
        chrome_options.add_argument('--disable-gpu')
        chrome_options.add_argument('--no-sandbox')

        # Launch browser
        driver = webdriver.Chrome(options=chrome_options)

        # Load HTML file
        html_url = f"file://{os.path.abspath(html_path)}"
        driver.get(html_url)

        # Wait for page to load
        import time
        time.sleep(3)  # Wait for Plotly charts

        # Take screenshot
        driver.save_screenshot(output_path)

        driver.quit()
        return True
    except Exception as e:
        print(f"❌ Selenium screenshot failed: {e}")
        return False


def screenshot_with_imgkit(html_path, output_path, width=1600, height=2400):
    """Take screenshot using imgkit/wkhtmltoimage (another fallback)"""
    try:
        import imgkit
    except ImportError:
        print("❌ imgkit not installed")
        return False

    try:
        options = {
            'width': width,
            'height': height,
            'enable-javascript': None,
            'javascript-delay': 2000,
            'quality': 100
        }

        imgkit.from_file(html_path, output_path, options=options)
        return True
    except Exception as e:
        print(f"❌ imgkit screenshot failed: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(description='Screenshot dashboard HTML file')
    parser.add_argument('--dashboard', required=True, help='Path to dashboard HTML file')
    parser.add_argument('--output', required=True, help='Output PNG file path')
    parser.add_argument('--width', type=int, default=1600, help='Screenshot width (default: 1600)')
    parser.add_argument('--height', type=int, default=2400, help='Screenshot height (default: 2400)')
    parser.add_argument('--method', choices=['playwright', 'selenium', 'imgkit', 'auto'],
                       default='auto', help='Screenshot method (default: auto)')

    args = parser.parse_args()

    # Validate input file
    if not os.path.exists(args.dashboard):
        print(f"❌ Dashboard file not found: {args.dashboard}")
        return 1

    print(f"📸 Taking screenshot of dashboard...")
    print(f"   Input: {args.dashboard}")
    print(f"   Output: {args.output}")
    print(f"   Size: {args.width}x{args.height}")
    print()

    success = False

    if args.method == 'auto':
        # Try methods in order of preference
        print("🔍 Trying Playwright (best quality)...")
        success = screenshot_with_playwright(args.dashboard, args.output, args.width, args.height)

        if not success:
            print("🔍 Trying Selenium (fallback)...")
            success = screenshot_with_selenium(args.dashboard, args.output, args.width, args.height)

        if not success:
            print("🔍 Trying imgkit (last resort)...")
            success = screenshot_with_imgkit(args.dashboard, args.output, args.width, args.height)

    elif args.method == 'playwright':
        success = screenshot_with_playwright(args.dashboard, args.output, args.width, args.height)
    elif args.method == 'selenium':
        success = screenshot_with_selenium(args.dashboard, args.output, args.width, args.height)
    elif args.method == 'imgkit':
        success = screenshot_with_imgkit(args.dashboard, args.output, args.width, args.height)

    if success and os.path.exists(args.output):
        file_size = os.path.getsize(args.output) / 1024  # KB
        print(f"\n✅ Screenshot saved: {args.output}")
        print(f"   Size: {file_size:.1f} KB")
        return 0
    else:
        print(f"\n❌ Failed to create screenshot")
        print("\nInstallation instructions:")
        print("  pip install playwright")
        print("  playwright install chromium")
        print("\nOr:")
        print("  pip install selenium")
        print("  # Install Chrome browser")
        return 1


if __name__ == '__main__':
    sys.exit(main())
