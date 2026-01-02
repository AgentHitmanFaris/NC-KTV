# NC-KTV Technical Documentation

Comprehensive technical documentation covering the internal architecture, mathematical models, and algorithms used in NC-KTV.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Timing Synchronization System](#timing-synchronization-system)
3. [Audio Processing Pipeline](#audio-processing-pipeline)
4. [Subtitle Format Support](#subtitle-format-support)
5. [Effect Compositor](#effect-compositor)
6. [Karaoke Animation Mathematics](#karaoke-animation-mathematics)
7. [Timeline Data Model](#timeline-data-model)
8. [Video Export Engine](#video-export-engine)

---

## Architecture Overview

NC-KTV follows a modular architecture with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────────┐
│                        User Interface                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │ Wizard Mode  │  │ Editor Mode  │  │  Timeline Widget     │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      Core Processing                            │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌─────────────┐  │
│  │AudioClock │  │Effect     │  │Subtitle   │  │Video Export │  │
│  │           │  │Compositor │  │Parser     │  │Engine       │  │
│  └───────────┘  └───────────┘  └───────────┘  └─────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                    External Libraries                           │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────────┐ │
│  │ UVR/MDX  │  │ Whisper  │  │ FFmpeg   │  │ PyQt6/Multimedia│ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

---

## Timing Synchronization System

### Overview

The timing system ensures sample-accurate synchronization between:
- Audio playback
- Video frames
- Lyrics display
- Effect animations

### AudioClock Mathematics

The `AudioClock` class (src/core/audio_clock.py) provides the master timing reference:

#### Time Conversion Formulas

**Samples to Seconds:**
```
t_seconds = n_samples / sample_rate
```

**Seconds to Samples:**
```
n_samples = t_seconds × sample_rate
```

**Example:** At 48kHz sample rate:
- 48,000 samples = 1.0 second
- 1,440,000 samples = 30.0 seconds

#### Latency Compensation

Total corrected time accounts for processing latencies:

```
t_corrected = t_raw + offset_master + Σ(latency_corrections[stage])
```

Where:
- `t_raw` = Raw timestamp from source
- `offset_master` = User-calibrated global offset (±5 seconds)
- `latency_corrections` = Per-stage corrections {uvr, transcription, playback, preview}

#### Sample Rate Validation

All audio files must share the same sample rate to avoid drift:

```
drift_per_second = |1 - (sample_rate_source / sample_rate_target)|
```

For a 3-minute song with 44.1kHz source vs 48kHz target:
```
drift = |1 - (44100/48000)| × 180s = 0.08125 × 180 = 14.6 seconds drift!
```

---

## Audio Processing Pipeline

### UVR Separation

The vocal separation uses:
- **VR Models (5_HP, 6_HP)**: Traditional spectrogram-based separation
- **MDX-Net (KARA_2)**: Deep neural network for higher quality

#### Spectrogram Representation

Audio is converted to Short-Time Fourier Transform (STFT):

```
X(k, m) = Σ x(n) × w(n - mH) × e^(-j2πkn/N)
```

Where:
- `x(n)` = Input audio samples
- `w(n)` = Window function (Hann)
- `H` = Hop size (typically 512-1024 samples)
- `N` = FFT size (typically 2048-4096)
- `k` = Frequency bin index
- `m` = Time frame index

### Cross-Correlation for Delay Measurement

To measure UVR processing delay between original and separated audio:

```
R_xy(τ) = Σ x(n) × y(n + τ)

delay_samples = argmax(R_xy)
delay_seconds = delay_samples / sample_rate
```

---

## Subtitle Format Support

NC-KTV supports multiple subtitle formats with unified parsing:

### Supported Formats

| Format | Extension | Timestamp Format | Example |
|--------|-----------|------------------|---------|
| SubRip | .srt | `HH:MM:SS,mmm` | `00:01:23,456` |
| LRC | .lrc | `[MM:SS.xx]` | `[01:23.45]` |
| WebVTT | .vtt | `HH:MM:SS.mmm` | `00:01:23.456` |
| TTML | .ttml, .dfxp | `HH:MM:SS:FF` or `123.456s` | XML attributes |
| ASS/SSA | .ass, .ssa | `H:MM:SS.cc` | `0:01:23.45` |

### Timestamp Parsing Formulas

**SRT/VTT (HH:MM:SS.mmm):**
```
t_seconds = hours × 3600 + minutes × 60 + seconds + milliseconds / 1000
```

**LRC (MM:SS.xx):**
```
t_seconds = minutes × 60 + seconds + centiseconds / 100
```

**ASS/SSA (H:MM:SS.cc):**
```
t_seconds = hours × 3600 + minutes × 60 + seconds + centiseconds / 100
```

---

## Effect Compositor

### Bezier Curve Evaluation

The `EffectCompositor` (src/core/effect_compositor.py) uses cubic Bezier curves for smooth animations:

#### Cubic Bezier Formula

```
B(t) = (1-t)³P₀ + 3(1-t)²tP₁ + 3(1-t)t²P₂ + t³P₃
```

Where t ∈ [0, 1] and P₀, P₁, P₂, P₃ are control points.

#### Standard Easing Presets

| Preset | P₀ | P₁ | P₂ | P₃ |
|--------|----|----|----|----|
| Linear | (0,0) | (0.33,0.33) | (0.67,0.67) | (1,1) |
| Ease-In | (0,0) | (0.42,0) | (1,1) | (1,1) |
| Ease-Out | (0,0) | (0,0) | (0.58,1) | (1,1) |
| Ease-In-Out | (0,0) | (0.42,0) | (0.58,1) | (1,1) |

#### Progress Calculation

For an effect with duration `[start_time, end_time]` at time `t`:

```
progress_linear = (t - start_time) / (end_time - start_time)
progress_eased = evaluate_bezier(progress_linear)
```

### Effect Value Interpolation

Each effect type calculates its value from eased progress:

**Opacity (Fade In/Out):**
```
opacity = progress_eased × 255  (fade in)
opacity = (1 - progress_eased) × 255  (fade out)
```

**Translation (Slide):**
```
x = start_x + progress_eased × (end_x - start_x)
y = start_y + progress_eased × (end_y - start_y)
```

**Scale (Zoom):**
```
scale = start_scale + progress_eased × (end_scale - start_scale)
```

**Blur (Gaussian):**
```
radius = start_radius + progress_eased × (end_radius - start_radius)
```

---

## Karaoke Animation Mathematics

### Linear Wipe (Fill Progress)

The classic karaoke effect fills text left-to-right based on playback position:

```
fill_progress = (current_time - line_start) / (line_end - line_start)
fill_width = text_width × clamp(fill_progress, 0, 1)
```

### Word-Level Timing

For word-level synchronization:

```
word_progress = (current_time - word_start) / (word_end - word_start)
word_highlighted = word_progress >= 0 && word_progress <= 1
```

### Bouncing Ball Animation

The bouncing ball follows a parabolic trajectory between words:

```
For word at position (x, y):
  ball_x = word_x + word_width / 2
  ball_y = baseline_y - amplitude × sin(π × inter_word_progress)
```

Where `inter_word_progress` ∈ [0, 1] represents travel between consecutive words.

### Glow Pulse Effect

Pulsing glow uses sinusoidal intensity:

```
glow_intensity = base_intensity + amplitude × |sin(2π × frequency × t)|
glow_radius = base_radius × (1 + intensity_factor × glow_intensity)
```

---

## Timeline Data Model

### Data Structures

```
Timeline
├── tracks: List<Track>
│   ├── track_type: "audio" | "video" | "effects" | "lyrics"
│   ├── name: str
│   └── clips: List<Clip>
│       ├── id: str (UUID)
│       ├── start_time: float (seconds)
│       ├── duration: float (seconds)
│       ├── data: dict (clip-specific data)
│       └── effects: List<Effect>
│           ├── effect_type: str
│           ├── start_time: float (relative to clip)
│           ├── duration: float
│           ├── properties: dict
│           └── easing: EasingCurve
└── duration: float (total timeline length)
```

### Clip Operations

**Move Clip:**
```
new_start = max(0, old_start + delta_time)
```

**Resize Clip (from end):**
```
new_duration = max(MIN_DURATION, old_duration + delta_time)
```

**Split Clip at Playhead:**
```
clip1.duration = split_time - clip.start_time
clip2.start_time = split_time
clip2.duration = clip.end_time - split_time
```

### Overlap Detection

```
overlaps(clip_a, clip_b) = 
  (clip_a.start < clip_b.end) AND (clip_b.start < clip_a.end)
```

### Snap-to-Grid

```
snapped_time = round(time / grid_size) × grid_size
```

Common grid sizes: 0.1s, 0.25s, 0.5s, 1.0s

---

## Video Export Engine

### Frame Rendering Pipeline

1. **Background Layer**: Original video frame or solid color
2. **Lyrics Layer**: Rendered text with effects
3. **Composition**: Alpha blending of layers

### Text Rendering Mathematics

**Center Alignment:**
```
text_x = (frame_width - text_width) / 2
text_y = frame_height × vertical_position
```

**Outline (Stroke):**
```
For each pixel at (x, y):
  distance = distance_to_glyph_edge(x, y)
  if distance < outline_width:
    color = outline_color
  elif distance < 0:
    color = fill_color
```

### Color Processing

**HSL to RGB Conversion (for color shift effect):**
```
C = (1 - |2L - 1|) × S
X = C × (1 - |((H/60) mod 2) - 1|)
m = L - C/2

(R', G', B') = based on H sector
(R, G, B) = ((R'+m)×255, (G'+m)×255, (B'+m)×255)
```

### Export Parameters

| Quality | Resolution | Video Bitrate | Audio Bitrate | Codec |
|---------|------------|---------------|---------------|-------|
| Draft | 720p | 2 Mbps | 128 kbps | H.264 |
| Standard | 1080p | 8 Mbps | 192 kbps | H.264 |
| High | 1080p | 15 Mbps | 320 kbps | H.265 |
| 4K | 2160p | 30 Mbps | 320 kbps | H.265 |

---

## Performance Optimization

### Memory Management

- **Audio Buffers**: Loaded in chunks (typically 10-30 seconds)
- **Video Frames**: Decoded on-demand with frame caching
- **Effect Cache**: Memoized effect calculations by time key

### Threading Model

```
Main Thread (UI)
├── Event Loop
├── Widget Updates
└── Preview Rendering

Worker Thread Pool
├── Audio Loading/Processing
├── Video Export
└── AI Transcription
```

### GPU Acceleration

CUDA is utilized for:
- UVR model inference (5-10x speedup)
- Video encoding (NVENC)
- Large FFT operations

---

## File Format (.nctv)

### Structure

```
NCTV File (Binary, Encrypted)
├── Magic Header: "NCTV" (4 bytes)
├── Version: uint16
├── Metadata Chunk
│   ├── Project name
│   ├── Creation date
│   └── Settings
├── Lyrics Chunk
│   └── JSON-serialized LyricsData
├── Timeline Chunk
│   └── JSON-serialized TimelineData
├── Media Chunk (embedded or reference)
│   ├── Source file
│   ├── Instrumental
│   └── Vocals
└── Checksum (SHA-256)
```

### Encryption

- Algorithm: AES-256-GCM
- Key derivation: PBKDF2 with machine-specific salt
- IV: Random 12 bytes per file

---

## API Reference

### Core Classes

| Class | Module | Purpose |
|-------|--------|---------|
| `AudioClock` | core.audio_clock | Sample-accurate timing |
| `EffectCompositor` | core.effect_compositor | Effect value calculation |
| `TimelineData` | core.timeline_data | Timeline data model |
| `LyricsData` | sync.sync_data | Lyrics storage |
| `VideoExporter` | core.video_export | Video rendering |

### Key Methods

**AudioClock:**
- `samples_to_seconds(samples)` → float
- `apply_sync_correction(time, stage)` → float
- `validate_sample_rates(files)` → dict

**EffectCompositor:**
- `calculate_effect(effect, current_time)` → dict
- `evaluate_bezier(t, curve)` → float
- `apply_to_painter(painter, effect_values)`

---

## Glossary

| Term | Definition |
|------|------------|
| **BPM** | Beats Per Minute - tempo measurement |
| **CUDA** | NVIDIA GPU compute platform |
| **FFT** | Fast Fourier Transform |
| **Latency** | Time delay in audio processing |
| **MDX-Net** | Music Demixing neural network |
| **Sample Rate** | Audio samples per second (Hz) |
| **STFT** | Short-Time Fourier Transform |
| **UVR** | Ultimate Vocal Remover |
| **VR Model** | Vocal Remover model architecture |

---

*Last updated: January 3, 2026*
