#!/usr/bin/env python3
"""Windows Service Unquoted Path Auditor - Compact Security Script"""

import subprocess, os, re

def audit_services():
    """Check for unquoted service paths with write access."""
    try:
        result = subprocess.run(
            ["wmic", "service", "get", "name,displayname,pathname,startmode"],
            capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW
        )
    except Exception:
        # Fallback for non-Windows or wmic unavailable
        print("WMIC unavailable. Run on Windows with admin privileges.")
        return

    vulnerable = []
    for line in result.stdout.splitlines()[1:]:  # Skip header
        if not line.strip():
            continue
        parts = line.strip().split(None, 3)
        if len(parts) < 4:
            continue
        pathname = parts[3] if len(parts) > 3 else ""
        
        # Check: has spaces, not quoted, executable path
        if ' ' in pathname and not pathname.startswith('"') and pathname.lower().endswith('.exe'):
            # Extract directory from path
            match = re.match(r'^(.+\\)[^\\]+$', pathname)
            if match:
                dir_path = match.group(1).strip()
                # Check write access
                if os.access(dir_path, os.W_OK):
                    vulnerable.append(pathname)

    if vulnerable:
        print("[!] POTENTIAL PRIVILEGE ESCALATION VECTORS FOUND:")
        for path in vulnerable:
            print(f"    {path}")
    else:
        print("[+] No vulnerable unquoted service paths detected.")

if __name__ == "__main__":
    audit_services()
