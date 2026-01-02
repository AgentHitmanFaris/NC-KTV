"""
Universal Subtitle Parser for NC-KTV
Supports: SRT, LRC, VTT, TTML/DFXP, ASS/SSA
"""

import re
import sys
from pathlib import Path
from typing import List, Optional
import xml.etree.ElementTree as ET

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from sync.sync_data import LyricsData, LyricLine


def detect_subtitle_format(file_path: Path) -> str:
    """Detect subtitle format from file extension and content"""
    ext = file_path.suffix.lower()
    
    format_map = {
        '.srt': 'srt',
        '.lrc': 'lrc',
        '.vtt': 'vtt',
        '.ttml': 'ttml',
        '.dfxp': 'ttml',
        '.xml': 'ttml',  # May need content inspection
        '.ass': 'ass',
        '.ssa': 'ass',
    }
    
    return format_map.get(ext, 'unknown')


def import_subtitle(file_path: Path) -> LyricsData:
    """
    Import subtitles from any supported format
    
    Supported formats:
    - SRT (.srt) - SubRip
    - LRC (.lrc) - Lyrics
    - VTT (.vtt) - WebVTT
    - TTML/DFXP (.ttml, .dfxp, .xml) - Timed Text Markup Language
    - ASS/SSA (.ass, .ssa) - Advanced SubStation Alpha
    """
    file_path = Path(file_path)
    format_type = detect_subtitle_format(file_path)
    
    if format_type == 'srt':
        from utils.srt_parser import import_srt
        return import_srt(file_path)
    elif format_type == 'lrc':
        from utils.lrc_parser import parse_lrc_file
        return parse_lrc_file(file_path)
    elif format_type == 'vtt':
        return import_vtt(file_path)
    elif format_type == 'ttml':
        return import_ttml(file_path)
    elif format_type == 'ass':
        return import_ass(file_path)
    else:
        raise ValueError(f"Unsupported subtitle format: {file_path.suffix}")


# ============== VTT Parser ==============

def parse_vtt_timestamp(timestamp: str) -> float:
    """
    Parse VTT timestamp to seconds
    Format: HH:MM:SS.mmm or MM:SS.mmm
    """
    parts = timestamp.strip().split(':')
    
    if len(parts) == 3:
        # HH:MM:SS.mmm
        hours = int(parts[0])
        minutes = int(parts[1])
        seconds = float(parts[2])
        return hours * 3600 + minutes * 60 + seconds
    elif len(parts) == 2:
        # MM:SS.mmm
        minutes = int(parts[0])
        seconds = float(parts[1])
        return minutes * 60 + seconds
    else:
        return 0.0


def import_vtt(file_path: Path) -> LyricsData:
    """
    Import WebVTT subtitle file
    
    VTT Format:
    WEBVTT
    
    00:00:01.000 --> 00:00:04.000
    First subtitle
    
    00:00:05.000 --> 00:00:08.000
    Second subtitle
    """
    lyrics_data = LyricsData()
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Normalize line endings
    content = content.replace('\r\n', '\n').replace('\r', '\n')
    
    # Split into blocks
    blocks = re.split(r'\n\n+', content.strip())
    
    for block in blocks:
        # Skip WEBVTT header and NOTE blocks
        if block.startswith('WEBVTT') or block.startswith('NOTE'):
            continue
        
        lines = block.strip().split('\n')
        
        # Find timestamp line
        timestamp_idx = -1
        for i, line in enumerate(lines):
            if '-->' in line:
                timestamp_idx = i
                break
        
        if timestamp_idx == -1:
            continue
        
        timestamp_line = lines[timestamp_idx]
        text_lines = lines[timestamp_idx + 1:]
        
        # Parse timestamps (may have positioning info after)
        match = re.match(r'([\d:\.]+)\s*-->\s*([\d:\.]+)', timestamp_line)
        if not match:
            continue
        
        start_str, end_str = match.groups()
        start_time = parse_vtt_timestamp(start_str)
        end_time = parse_vtt_timestamp(end_str)
        
        # Join text, remove VTT tags like <c>, <v>, etc.
        text = ' '.join(line.strip() for line in text_lines if line.strip())
        text = re.sub(r'<[^>]+>', '', text)
        
        if not text:
            continue
        
        lyrics_data.lines.append(LyricLine(
            text=text,
            start_time=start_time,
            end_time=end_time
        ))
    
    return lyrics_data


# ============== TTML/DFXP Parser ==============

def parse_ttml_timestamp(timestamp: str) -> float:
    """
    Parse TTML timestamp to seconds
    Formats: 
    - HH:MM:SS.mmm
    - HH:MM:SS:FF (frames)
    - 123.456s
    - 123456ms
    """
    timestamp = timestamp.strip()
    
    # Format: 123.456s
    if timestamp.endswith('s'):
        return float(timestamp[:-1])
    
    # Format: 123456ms
    if timestamp.endswith('ms'):
        return float(timestamp[:-2]) / 1000.0
    
    # Format: HH:MM:SS.mmm or HH:MM:SS:FF
    parts = re.split(r'[:.]', timestamp)
    if len(parts) >= 3:
        hours = int(parts[0])
        minutes = int(parts[1])
        seconds = int(parts[2])
        ms = int(parts[3]) if len(parts) > 3 else 0
        
        # If ms > 999, it's probably frames (assume 30fps)
        if ms > 999:
            ms = int(ms / 30 * 1000)
        
        return hours * 3600 + minutes * 60 + seconds + ms / 1000.0
    
    return 0.0


def import_ttml(file_path: Path) -> LyricsData:
    """
    Import TTML/DFXP subtitle file (XML-based)
    
    Used by YouTube, Netflix, and other streaming services.
    """
    lyrics_data = LyricsData()
    
    try:
        tree = ET.parse(file_path)
        root = tree.getroot()
        
        # Handle namespace
        ns = {}
        if root.tag.startswith('{'):
            ns_uri = root.tag.split('}')[0][1:]
            ns = {'tt': ns_uri}
        
        # Find all <p> elements (paragraphs/subtitles)
        # Try different namespace patterns
        p_elements = []
        
        # Try with namespace
        for ns_prefix in ['tt:', 'ttml:', '']:
            p_elements = root.findall(f'.//{ns_prefix}p', ns) if ns else root.findall(f'.//{ns_prefix}p')
            if p_elements:
                break
        
        # Fallback: find all elements named 'p'
        if not p_elements:
            p_elements = [elem for elem in root.iter() if elem.tag.endswith('p')]
        
        for p in p_elements:
            # Get timing attributes
            begin = p.get('begin') or p.get('start') or '0'
            end = p.get('end') or p.get('dur') or '0'
            
            # Handle duration instead of end time
            if p.get('dur') and not p.get('end'):
                start_time = parse_ttml_timestamp(begin)
                duration = parse_ttml_timestamp(end)
                end_time = start_time + duration
            else:
                start_time = parse_ttml_timestamp(begin)
                end_time = parse_ttml_timestamp(end)
            
            # Get text content (may have nested spans)
            text = ''.join(p.itertext()).strip()
            text = re.sub(r'\s+', ' ', text)  # Normalize whitespace
            
            if not text:
                continue
            
            lyrics_data.lines.append(LyricLine(
                text=text,
                start_time=start_time,
                end_time=end_time
            ))
        
    except ET.ParseError as e:
        raise ValueError(f"Invalid TTML/XML file: {e}")
    
    return lyrics_data


# ============== ASS/SSA Parser ==============

def parse_ass_timestamp(timestamp: str) -> float:
    """
    Parse ASS/SSA timestamp to seconds
    Format: H:MM:SS.cc (centiseconds)
    """
    match = re.match(r'(\d+):(\d{2}):(\d{2})\.(\d{2})', timestamp.strip())
    if not match:
        return 0.0
    
    hours, minutes, seconds, centiseconds = match.groups()
    
    return (
        int(hours) * 3600 +
        int(minutes) * 60 +
        int(seconds) +
        int(centiseconds) / 100.0
    )


def import_ass(file_path: Path) -> LyricsData:
    """
    Import ASS/SSA subtitle file
    
    ASS Format:
    [Events]
    Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
    Dialogue: 0,0:00:01.00,0:00:04.00,Default,,0,0,0,,First subtitle
    """
    lyrics_data = LyricsData()
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Normalize line endings
    content = content.replace('\r\n', '\n').replace('\r', '\n')
    
    # Find [Events] section
    in_events = False
    format_fields = []
    
    for line in content.split('\n'):
        line = line.strip()
        
        if line == '[Events]':
            in_events = True
            continue
        
        if line.startswith('[') and in_events:
            break  # End of Events section
        
        if not in_events:
            continue
        
        # Parse Format line
        if line.startswith('Format:'):
            format_str = line[7:].strip()
            format_fields = [f.strip().lower() for f in format_str.split(',')]
            continue
        
        # Parse Dialogue line
        if line.startswith('Dialogue:'):
            dialogue_str = line[9:].strip()
            
            # Split by comma, but text field may contain commas
            parts = dialogue_str.split(',', len(format_fields) - 1)
            
            if len(parts) < len(format_fields):
                continue
            
            # Map to field names
            fields = dict(zip(format_fields, parts))
            
            start_time = parse_ass_timestamp(fields.get('start', '0:00:00.00'))
            end_time = parse_ass_timestamp(fields.get('end', '0:00:00.00'))
            text = fields.get('text', '')
            
            # Remove ASS style tags like {\b1}, {\an8}, etc.
            text = re.sub(r'\{[^}]*\}', '', text)
            # Convert \N to space (line break in ASS)
            text = text.replace('\\N', ' ').replace('\\n', ' ')
            text = text.strip()
            
            if not text:
                continue
            
            lyrics_data.lines.append(LyricLine(
                text=text,
                start_time=start_time,
                end_time=end_time
            ))
    
    return lyrics_data


# ============== Export Functions ==============

def export_subtitle(lyrics_data: LyricsData, output_path: Path, format_type: str = None):
    """Export lyrics to subtitle format"""
    output_path = Path(output_path)
    
    if format_type is None:
        format_type = detect_subtitle_format(output_path)
    
    if format_type == 'srt':
        from utils.srt_parser import export_srt
        export_srt(lyrics_data, output_path)
    elif format_type == 'vtt':
        export_vtt(lyrics_data, output_path)
    elif format_type == 'ass':
        export_ass(lyrics_data, output_path)
    else:
        raise ValueError(f"Export not supported for format: {format_type}")


def export_vtt(lyrics_data: LyricsData, output_path: Path):
    """Export to WebVTT format"""
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("WEBVTT\n\n")
        
        for i, line in enumerate(lyrics_data.lines, start=1):
            # Format timestamp as HH:MM:SS.mmm
            start = format_vtt_time(line.start_time)
            end = format_vtt_time(line.end_time)
            
            f.write(f"{i}\n")
            f.write(f"{start} --> {end}\n")
            f.write(f"{line.text}\n\n")


def format_vtt_time(seconds: float) -> str:
    """Format seconds to VTT timestamp"""
    hours = int(seconds // 3600)
    seconds %= 3600
    minutes = int(seconds // 60)
    seconds %= 60
    secs = int(seconds)
    ms = int((seconds - secs) * 1000)
    return f"{hours:02d}:{minutes:02d}:{secs:02d}.{ms:03d}"


def export_ass(lyrics_data: LyricsData, output_path: Path):
    """Export to ASS format"""
    with open(output_path, 'w', encoding='utf-8') as f:
        # Write header
        f.write("[Script Info]\n")
        f.write("Title: NC-KTV Export\n")
        f.write("ScriptType: v4.00+\n")
        f.write("Collisions: Normal\n\n")
        
        # Write styles
        f.write("[V4+ Styles]\n")
        f.write("Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n")
        f.write("Style: Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,-1,0,0,0,100,100,0,0,1,2,1,2,10,10,30,1\n\n")
        
        # Write events
        f.write("[Events]\n")
        f.write("Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n")
        
        for line in lyrics_data.lines:
            start = format_ass_time(line.start_time)
            end = format_ass_time(line.end_time)
            f.write(f"Dialogue: 0,{start},{end},Default,,0,0,0,,{line.text}\n")


def format_ass_time(seconds: float) -> str:
    """Format seconds to ASS timestamp (H:MM:SS.cc)"""
    hours = int(seconds // 3600)
    seconds %= 3600
    minutes = int(seconds // 60)
    seconds %= 60
    secs = int(seconds)
    cs = int((seconds - secs) * 100)
    return f"{hours}:{minutes:02d}:{secs:02d}.{cs:02d}"


# ============== Supported Formats List ==============

SUPPORTED_FORMATS = {
    'srt': {'name': 'SubRip', 'extensions': ['.srt'], 'import': True, 'export': True},
    'lrc': {'name': 'LRC Lyrics', 'extensions': ['.lrc'], 'import': True, 'export': True},
    'vtt': {'name': 'WebVTT', 'extensions': ['.vtt'], 'import': True, 'export': True},
    'ttml': {'name': 'TTML/DFXP', 'extensions': ['.ttml', '.dfxp', '.xml'], 'import': True, 'export': False},
    'ass': {'name': 'ASS/SSA', 'extensions': ['.ass', '.ssa'], 'import': True, 'export': True},
}


def get_import_filter() -> str:
    """Get file filter string for import dialog"""
    all_exts = []
    filters = []
    
    for fmt, info in SUPPORTED_FORMATS.items():
        if info['import']:
            exts = ' '.join(f'*{ext}' for ext in info['extensions'])
            filters.append(f"{info['name']} ({exts})")
            all_exts.extend(info['extensions'])
    
    all_exts_str = ' '.join(f'*{ext}' for ext in all_exts)
    return f"All Subtitles ({all_exts_str});;" + ';;'.join(filters) + ";;All Files (*.*)"
