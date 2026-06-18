import sys
import datetime
import os

def main():
    if len(sys.argv) < 2:
        print("Usage: python generate_version.py <output_file>")
        sys.exit(1)
        
    output_path = sys.argv[1]
    
    # Calculate Malaysia timezone (UTC+8) time
    utc_now = datetime.datetime.now(datetime.timezone.utc)
    my_now = utc_now + datetime.timedelta(hours=8)
    
    # Format: 2.1.dMyyHHmm
    # d is unpadded day
    # M is unpadded month
    # yy is 2-digit year
    # HH is padded hour
    # mm is padded minute
    version_suffix = f"{my_now.day}{my_now.month}{my_now.year % 100:02d}{my_now.hour:02d}{my_now.minute:02d}"
    version_str = f"2.1.{version_suffix}"
    
    content = f'#pragma once\n\n#define CURRENT_VERSION "{version_str}"\n'
    
    # Check if existing file matches content to prevent rebuilds
    if os.path.exists(output_path):
        with open(output_path, "r", encoding="utf-8") as f:
            old_content = f.read()
        if old_content == content:
            # Identical content, do nothing to preserve timestamp
            print(f"[VERSION] Version is unchanged: {version_str}")
            return
            
    # Write new version
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"[VERSION] Generated new version header: {version_str}")

if __name__ == "__main__":
    main()
