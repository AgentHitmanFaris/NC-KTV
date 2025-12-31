"""
ASS (Advanced Substation Alpha) Generator for Karaoke
"""
from core.lyrics import LyricsData

class ASSGenerator:
    def __init__(self, lyrics_data: LyricsData, style: str = "Neon Gold", animation: str = "Standard (Wipe)"):
        self.lyrics = lyrics_data
        self.style = style
        self.animation = animation
        
    def generate(self) -> str:
        """Generate ASS file content"""
        header = self._generate_header()
        styles = self._generate_styles()
        events = self._generate_events()
        
        return f"{header}\n{styles}\n{events}"
        
    def _generate_header(self):
        return """[Script Info]
Title: NC-KTV Karaoke
ScriptType: v4.00+
WrapStyle: 0
ScaledBorderAndShadow: yes
YCbCr Matrix: TV.601
PlayResX: 1920
PlayResY: 1080
"""

    def _generate_styles(self):
        """Define Styles based on selection"""
        base_header = """[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding"""
        
        if self.style == "Neon Gold":
            # Current Neon Style
            return base_header + """
Style: NeonSharp,Arial,60,&H0000D7FF,&H00FFFFFF,&H0000D7FF,&H00000000,1,0,0,0,100,100,0,0,1,1.5,0,2,20,20,50,1
Style: NeonBlur,Arial,60,&H0000D7FF,&H0000D7FF,&H0000D7FF,&H00000000,1,0,0,0,100,100,0,0,1,3,0,2,20,20,50,1
"""
        elif self.style == "Classic Blue":
            # Classic Karaoke: White text turns Blue
            # Primary: Blue (&H00FF0000) (BGR)
            # Secondary: White (&H00FFFFFF)
            # Outline: Dark Blue/Black
            return base_header + """
Style: Classic,Arial,60,&H00FF0000,&H00FFFFFF,&H00400000,&H00000000,1,0,0,0,100,100,0,0,1,2,0,2,20,20,50,1
"""
        elif self.style == "Clean White":
            # Minimalist: Grey turns White
            # Primary: White (&H00FFFFFF)
            # Secondary: Light Grey (&H00BEBEBE)
            return base_header + """
Style: Clean,Arial,60,&H00FFFFFF,&H00BEBEBE,&H00000000,&H00000000,1,0,0,0,100,100,0,0,1,2,0,2,20,20,50,1
"""
        else:
             # Fallback
             return base_header + """
Style: Default,Arial,60,&H00FFFFFF,&H0000FFFF,&H00000000,&H00000000,1,0,0,0,100,100,0,0,1,2,0,2,10,10,50,1
"""

    def _generate_events(self):
        content = ["[Events]", "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text"]
        
        for line in self.lyrics.lines:
            start_fmt = self._format_time(line.start_time)
            end_fmt = self._format_time(line.end_time)
            
            # --- Karaoke Wipe Tag Construction ---
            k_text = ""
            if line.tokens:
                # Word-level
                current_time = line.start_time
                # Initial gap
                if line.tokens[0].start_time > current_time:
                    gap = line.tokens[0].start_time - current_time
                    k_text += f"{{\\k{int(gap*100)}}}"
                    current_time += gap
                    
                for token in line.tokens:
                    duration = token.end_time - token.start_time
                    k_val = int(duration * 100)
                    k_text += f"{{\\k{k_val}}}{token.text} "
            else:
                # Line-level
                duration = line.duration
                k_val = int(duration * 100)
                k_text = f"{{\\k{k_val}}}{line.text}"
            
            # --- Animation Tags ---
            anim_tags = ""
            if "Fade" in self.animation:
                anim_tags = "{\\fad(300,300)}"
            elif "Zoom" in self.animation:
                # Zoom in from 90% to 100% over 200ms
                anim_tags = "{\\fscx90\\fscy90\\t(0,200,\\fscx100\\fscy100)}"
            elif "Slide" in self.animation:
                # Slide Up from bottom (1080) to Margin V position (1030)
                # Center X is 960
                anim_tags = "{\\move(960,1080,960,1030)}"
            
            final_text = anim_tags + k_text

            # --- Event Generation based on Style ---
            
            if self.style == "Neon Gold":
                # Double Layer with Blur
                glow_line = f"Dialogue: 0,{start_fmt},{end_fmt},NeonBlur,,0,0,0,,{{\\blur15}}{final_text}"
                content.append(glow_line)
                sharp_line = f"Dialogue: 1,{start_fmt},{end_fmt},NeonSharp,,0,0,0,,{{\\blur1}}{final_text}"
                content.append(sharp_line)
                
            elif self.style == "Classic Blue":
                # Single Layer
                line_str = f"Dialogue: 0,{start_fmt},{end_fmt},Classic,,0,0,0,,{final_text}"
                content.append(line_str)
                
            elif self.style == "Clean White":
                # Single Layer
                line_str = f"Dialogue: 0,{start_fmt},{end_fmt},Clean,,0,0,0,,{final_text}"
                content.append(line_str)
            else:
                 # Default
                line_str = f"Dialogue: 0,{start_fmt},{end_fmt},Default,,0,0,0,,{final_text}"
                content.append(line_str)
            
        return "\n".join(content)

    def _format_time(self, seconds: float) -> str:
        # H:MM:SS.cs
        h = int(seconds // 3600)
        m = int((seconds % 3600) // 60)
        s = int(seconds % 60)
        cs = int((seconds % 1) * 100)
        return f"{h}:{m:02}:{s:02}.{cs:02}"
