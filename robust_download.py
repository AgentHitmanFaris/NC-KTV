import sys
from pathlib import Path
import requests

def download_file(url, dest):
    print(f"Checking {url}...")
    headers = {'User-Agent': 'Mozilla/5.0'}
    try:
        response = requests.get(url, stream=True, headers=headers, timeout=30)
        response.raise_for_status()
    except Exception as e:
        print(f"Failed to connect to {url}: {e}")
        return False

    total_size = int(response.headers.get('content-length', 0))
    print(f"Total size: {total_size / (1024*1024):.1f} MB")
    
    with open(dest, 'wb') as f:
        done = 0
        for data in response.iter_content(1024*1024):
            f.write(data)
            done += len(data)
            if total_size > 0:
                percent = int(100 * done / total_size)
                if percent % 10 == 0:
                    sys.stdout.write(f"\rDownloading {dest.name}... {percent}%")
                    sys.stdout.flush()
    print(f"\nFinished {dest.name}")
    return True

def main():
    root_dir = Path("D:/NC-KTV")
    uvr_dir = root_dir / "models" / "uvr"
    uvr_dir.mkdir(parents=True, exist_ok=True)
    
    # Try multiple sources for the best models
    targets = {
        "UVR-MDX-NET-Voc_FT.onnx": [
            "https://huggingface.co/Anjok07/UVR-Models/resolve/main/MDX-Net/UVR-MDX-NET-Voc_FT.onnx",
            "https://github.com/Anjok07/ultimatevocalremovergui/releases/download/v5.5.0/UVR-MDX-NET-Voc_FT.onnx"
        ],
        "Kim_Vocal_2.onnx": [
            "https://huggingface.co/Anjok07/UVR-Models/resolve/main/MDX-Net/Kim_Vocal_2.onnx",
            "https://github.com/Anjok07/ultimatevocalremovergui/releases/download/v5.5.0/Kim_Vocal_2.onnx"
        ]
    }
    
    for filename, urls in targets.items():
        dest = uvr_dir / filename
        if dest.exists() and dest.stat().st_size > 100 * 1024 * 1024:
            print(f"Already have {filename}")
            continue
            
        success = False
        for url in urls:
            if download_file(url, dest):
                success = True
                break
        if not success:
            print(f"Could not download {filename} from any source.")

if __name__ == "__main__":
    main()
