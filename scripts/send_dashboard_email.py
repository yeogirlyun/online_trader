#!/usr/bin/env python3
"""
Email Dashboard Notification Script
====================================

Sends trading dashboard HTML file via email with summary statistics.

Usage:
    python3 send_dashboard_email.py \
        --dashboard /path/to/dashboard.html \
        --trades /path/to/trades.jsonl \
        --recipient yeogirl@gmail.com

Requirements:
    pip install sendgrid

Environment:
    SENDGRID_API_KEY - SendGrid API key for sending emails
"""

import argparse
import json
import os
import sys
from datetime import datetime
from pathlib import Path


def load_trades(trades_file):
    """Load trade summary from JSONL file"""
    trades = []
    with open(trades_file, 'r') as f:
        for line in f:
            try:
                trades.append(json.loads(line))
            except:
                pass
    return trades


def calculate_summary(trades):
    """Calculate trading session summary"""
    if not trades:
        return {
            'total_trades': 0,
            'pnl': 0.0,
            'win_rate': 0.0,
            'final_equity': 100000.0
        }

    total_trades = len(trades)
    final_trade = trades[-1]

    # Extract metrics from final trade
    final_equity = final_trade.get('equity_after', 100000.0)
    pnl = final_equity - 100000.0
    pnl_pct = (pnl / 100000.0) * 100

    # Count wins
    wins = sum(1 for t in trades if t.get('pnl', 0) > 0)
    win_rate = (wins / total_trades * 100) if total_trades > 0 else 0

    return {
        'total_trades': total_trades,
        'pnl': pnl,
        'pnl_pct': pnl_pct,
        'win_rate': win_rate,
        'final_equity': final_equity
    }


def send_email_gmail_smtp(recipient, subject, body_html, dashboard_path, dashboard_image=None):
    """Send email using Gmail SMTP (requires app password)"""
    import smtplib
    from email.mime.multipart import MIMEMultipart
    from email.mime.text import MIMEText
    from email.mime.base import MIMEBase
    from email.mime.image import MIMEImage
    from email import encoders

    # Gmail SMTP settings
    smtp_server = "smtp.gmail.com"
    smtp_port = 587
    sender_email = os.environ.get('GMAIL_USER', 'your-email@gmail.com')
    sender_password = os.environ.get('GMAIL_APP_PASSWORD', '')

    if not sender_password:
        print("⚠️  Warning: GMAIL_APP_PASSWORD not set in environment")
        print("   Set it with: export GMAIL_APP_PASSWORD='your-app-password'")
        print("   Generate app password at: https://myaccount.google.com/apppasswords")
        return False

    # Create message with related parts (for embedded images)
    msg = MIMEMultipart('related')
    msg['From'] = sender_email
    msg['To'] = recipient
    msg['Subject'] = subject

    # Create alternative part for HTML
    msg_alternative = MIMEMultipart('alternative')
    msg.attach(msg_alternative)

    # Attach HTML body
    msg_alternative.attach(MIMEText(body_html, 'html'))

    # Embed dashboard image if provided
    if dashboard_image and os.path.exists(dashboard_image):
        with open(dashboard_image, 'rb') as f:
            img = MIMEImage(f.read())
            img.add_header('Content-ID', '<dashboard_image>')
            img.add_header('Content-Disposition', 'inline', filename='dashboard.png')
            msg.attach(img)

    # Attach dashboard HTML file
    if dashboard_path and os.path.exists(dashboard_path):
        with open(dashboard_path, 'rb') as f:
            part = MIMEBase('text', 'html')
            part.set_payload(f.read())
            encoders.encode_base64(part)
            part.add_header(
                'Content-Disposition',
                f'attachment; filename={os.path.basename(dashboard_path)}'
            )
            msg.attach(part)

    # Send email
    try:
        server = smtplib.SMTP(smtp_server, smtp_port)
        server.starttls()
        server.login(sender_email, sender_password)
        server.send_message(msg)
        server.quit()
        return True
    except Exception as e:
        print(f"❌ Failed to send email: {e}")
        return False


def create_email_body(summary, session_date, dashboard_filename, include_image=True):
    """Create HTML email body with trading summary and embedded dashboard image"""

    pnl_color = "green" if summary['pnl'] >= 0 else "red"
    pnl_sign = "+" if summary['pnl'] >= 0 else ""

    # Dashboard image section (only if image is included)
    dashboard_img_section = """
        <div class="dashboard-preview">
            <h2 style="margin-top: 0; color: #667eea;">📈 Session Dashboard</h2>
            <img src="cid:dashboard_image" alt="Trading Dashboard" style="width: 100%; max-width: 800px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1);">
            <p style="text-align: center; color: #7f8c8d; font-size: 13px; margin-top: 10px;">
                <i>Click the image or open the attachment for full interactive dashboard</i>
            </p>
        </div>
    """ if include_image else ""

    html = f"""
    <html>
    <head>
        <style>
            body {{
                font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
                line-height: 1.6;
                color: #333;
                max-width: 900px;
                margin: 0 auto;
                padding: 20px;
                background-color: #f8f9fa;
            }}
            .header {{
                background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                color: white;
                padding: 30px;
                border-radius: 10px;
                text-align: center;
                margin-bottom: 30px;
                box-shadow: 0 4px 6px rgba(0,0,0,0.1);
            }}
            .header h1 {{
                margin: 0;
                font-size: 28px;
            }}
            .header p {{
                margin: 10px 0 0 0;
                opacity: 0.9;
            }}
            .summary {{
                background: white;
                border-radius: 10px;
                padding: 25px;
                margin-bottom: 20px;
                box-shadow: 0 2px 4px rgba(0,0,0,0.05);
            }}
            .metric {{
                display: flex;
                justify-content: space-between;
                padding: 12px 0;
                border-bottom: 1px solid #e0e0e0;
            }}
            .metric:last-child {{
                border-bottom: none;
            }}
            .metric-label {{
                font-weight: 600;
                color: #555;
            }}
            .metric-value {{
                font-weight: 700;
                font-size: 18px;
            }}
            .pnl {{
                color: {pnl_color};
                font-size: 24px;
            }}
            .dashboard-preview {{
                background: white;
                border-radius: 10px;
                padding: 25px;
                margin-bottom: 20px;
                text-align: center;
                box-shadow: 0 2px 4px rgba(0,0,0,0.05);
            }}
            .footer {{
                text-align: center;
                color: #666;
                font-size: 14px;
                margin-top: 30px;
                padding-top: 20px;
                border-top: 1px solid #e0e0e0;
            }}
            .attachment {{
                background: #e3f2fd;
                border-left: 4px solid #2196F3;
                padding: 15px;
                margin: 20px 0;
                border-radius: 5px;
            }}
        </style>
    </head>
    <body>
        <div class="header">
            <h1>📊 OnlineTrader Session Report</h1>
            <p>{session_date}</p>
        </div>

        <div class="summary">
            <h2 style="margin-top: 0; color: #667eea;">Trading Summary</h2>

            <div class="metric">
                <span class="metric-label">Total Trades</span>
                <span class="metric-value">{summary['total_trades']}</span>
            </div>

            <div class="metric">
                <span class="metric-label">Win Rate</span>
                <span class="metric-value">{summary['win_rate']:.1f}%</span>
            </div>

            <div class="metric">
                <span class="metric-label">Final Equity</span>
                <span class="metric-value">${summary['final_equity']:,.2f}</span>
            </div>

            <div class="metric">
                <span class="metric-label">Session P&L</span>
                <span class="metric-value pnl">{pnl_sign}${summary['pnl']:,.2f} ({pnl_sign}{summary['pnl_pct']:.2f}%)</span>
            </div>
        </div>

        {dashboard_img_section}

        <div class="attachment">
            <strong>📎 Attachment:</strong> {dashboard_filename}<br>
            <small>Download and open the attached HTML file in your browser for the complete interactive dashboard with detailed charts and trade analysis.</small>
        </div>

        <div class="footer">
            <p>🤖 Generated by OnlineTrader v2.1</p>
            <p>Strategy: OnlineEnsemble EWRLS with Position State Machine</p>
        </div>
    </body>
    </html>
    """

    return html


def main():
    parser = argparse.ArgumentParser(description='Send trading dashboard via email')
    parser.add_argument('--dashboard', required=True, help='Path to dashboard HTML file')
    parser.add_argument('--trades', required=True, help='Path to trades JSONL file')
    parser.add_argument('--signals', help='Path to signals JSONL file')
    parser.add_argument('--positions', help='Path to positions JSONL file')
    parser.add_argument('--decisions', help='Path to decisions JSONL file')
    parser.add_argument('--recipient', default='yeogirl@gmail.com', help='Recipient email')
    parser.add_argument('--session-date', help='Session date (YYYY-MM-DD)')
    parser.add_argument('--no-image', action='store_true', help='Skip dashboard image generation')

    args = parser.parse_args()

    # Validate files exist
    if not os.path.exists(args.dashboard):
        print(f"❌ Dashboard file not found: {args.dashboard}")
        return 1

    if not os.path.exists(args.trades):
        print(f"❌ Trades file not found: {args.trades}")
        return 1

    # Determine session date and log directory
    if args.session_date:
        session_date = args.session_date
    else:
        # Extract from filename (e.g., session_20251009_162312.html)
        filename = os.path.basename(args.dashboard)
        try:
            date_part = filename.split('_')[1]  # 20251009
            session_date = f"{date_part[:4]}-{date_part[4:6]}-{date_part[6:8]}"
        except:
            session_date = datetime.now().strftime('%Y-%m-%d')

    # Auto-detect related files if not provided
    log_dir = os.path.dirname(args.trades)
    timestamp = os.path.basename(args.trades).replace('trades_', '').replace('.jsonl', '')

    if not args.signals:
        signals_path = os.path.join(log_dir, f'signals_{timestamp}.jsonl')
        args.signals = signals_path if os.path.exists(signals_path) else None

    if not args.positions:
        positions_path = os.path.join(log_dir, f'positions_{timestamp}.jsonl')
        args.positions = positions_path if os.path.exists(positions_path) else None

    if not args.decisions:
        decisions_path = os.path.join(log_dir, f'decisions_{timestamp}.jsonl')
        args.decisions = decisions_path if os.path.exists(decisions_path) else None

    print(f"📧 Preparing email notification...")
    print(f"   Dashboard: {args.dashboard}")
    print(f"   Trades: {args.trades}")
    print(f"   Signals: {args.signals or 'N/A'}")
    print(f"   Positions: {args.positions or 'N/A'}")
    print(f"   Decisions: {args.decisions or 'N/A'}")
    print(f"   Recipient: {args.recipient}")
    print(f"   Session: {session_date}")
    print()

    # Load and calculate summary
    trades = load_trades(args.trades)
    summary = calculate_summary(trades)

    print(f"📊 Session Summary:")
    print(f"   Trades: {summary['total_trades']}")
    print(f"   P&L: ${summary['pnl']:,.2f} ({summary['pnl_pct']:.2f}%)")
    print(f"   Final Equity: ${summary['final_equity']:,.2f}")
    print()

    # Generate dashboard screenshot
    dashboard_image = None
    if not args.no_image:
        print("📸 Taking dashboard screenshot...")
        dashboard_image = f"/tmp/dashboard_{timestamp}.png"

        # Screenshot the actual dashboard HTML file
        img_cmd = f"python3 tools/screenshot_dashboard.py " \
                  f"--dashboard {args.dashboard} " \
                  f"--output {dashboard_image} " \
                  f"--width 1600 " \
                  f"--height 3000"

        result = os.system(img_cmd + " 2>/dev/null")

        if result == 0 and os.path.exists(dashboard_image):
            print(f"✅ Dashboard screenshot saved: {dashboard_image}")
        else:
            print(f"⚠️  Dashboard screenshot failed (proceeding without image)")
            print(f"   Install Playwright: pip install playwright && playwright install chromium")
            dashboard_image = None
        print()

    # Create email content
    dashboard_filename = os.path.basename(args.dashboard)
    subject = f"OnlineTrader Report - {session_date} (P&L: {summary['pnl_pct']:+.2f}%)"
    body_html = create_email_body(summary, session_date, dashboard_filename, include_image=dashboard_image is not None)

    # Send email
    print("📤 Sending email...")
    success = send_email_gmail_smtp(args.recipient, subject, body_html, args.dashboard, dashboard_image)

    if success:
        print(f"✅ Email sent successfully to {args.recipient}")

        # Cleanup temporary dashboard image
        if dashboard_image and os.path.exists(dashboard_image):
            os.remove(dashboard_image)
            print(f"🗑️  Cleaned up temporary image")

        return 0
    else:
        print(f"❌ Failed to send email")
        return 1


if __name__ == '__main__':
    sys.exit(main())
