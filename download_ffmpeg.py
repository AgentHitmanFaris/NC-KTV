import urllib.request
import zipfile
import io
import os

url = 'https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip'
print('Downloading FFmpeg...')
req = urllib.request.urlopen(url)
zf = zipfile.ZipFile(io.BytesIO(req.read()))
os.makedirs('ffmpeg/bin', exist_ok=True)
for n in zf.namelist():
    if n.endswith('ffmpeg.exe') or n.endswith('ffprobe.exe'):
        b = os.path.basename(n)
        with open(os.path.join('ffmpeg', 'bin', b), 'wb') as f:
            f.write(zf.read(n))
        print(f"Extracted {b}")

print('Extraction complete!')
