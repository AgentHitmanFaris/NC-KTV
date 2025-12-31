"""
ASS (Advanced Substation Alpha) Generator for Karaoke
"""
from core.lyrics import LyricsData

class ASSGenerator:
    def __init__(self, lyrics_data: LyricsData, style: str = "Neon Gold", animation: str = "Standard (Wipe)"):
        self.lyrics = lyrics_data
        self.style = style
        self.animation = animation # Matches AnimationType values
        
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
        
        # Helper to get color codes
        # ASS Color is &HBBGGRR
        # &H00FFFFFF is White
        # &H000000FF is Red
        
        if self.animation == "Typewriter":
            # For Typewriter, Secondary Color (unfilled) must be TRANSPARENT (&HFF......)
            # So text "appears" as it fills
            return base_header + """
Style: Typewriter,Arial,60,&H00FFFFFF,&HFF000000,&H00000000,&H00000000,1,0,0,0,100,100,0,0,1,2,0,2,20,20,50,1
"""

        if self.style == "Neon Gold":
            return base_header + """
Style: NeonSharp,Arial,60,&H0000D7FF,&H00FFFFFF,&H0000D7FF,&H00000000,1,0,0,0,100,100,0,0,1,1.5,0,2,20,20,50,1
Style: NeonBlur,Arial,60,&H0000D7FF,&H0000D7FF,&H0000D7FF,&H00000000,1,0,0,0,100,100,0,0,1,3,0,2,20,20,50,1
"""
        elif self.style == "Classic Blue":
            return base_header + """
Style: Classic,Arial,60,&H00FF0000,&H00FFFFFF,&H00400000,&H00000000,1,0,0,0,100,100,0,0,1,2,0,2,20,20,50,1
"""
        elif self.style == "Clean White":
            return base_header + """
Style: Clean,Arial,60,&H00FFFFFF,&H00BEBEBE,&H00000000,&H00000000,1,0,0,0,100,100,0,0,1,2,0,2,20,20,50,1
"""
        else:
            return base_header + """
Style: Default,Arial,60,&H00FFFFFF,&H0000FFFF,&H00000000,&H00000000,1,0,0,0,100,100,0,0,1,2,0,2,10,10,50,1
"""

    def _generate_events(self):
        content = ["[Events]", "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text"]
        
        for line in self.lyrics.lines:
            start_fmt = self._format_time(line.start_time)
            end_fmt = self._format_time(line.end_time)
            
            # --- Map Preview Animations to ASS Format ---
            # Preview animations: "Linear Wipe", "Syllable Step", "Glow Pulse", "Fade In", "Bouncing Ball"
            
            # Karaoke tag type based on animation
            if self.animation == "Syllable Step":
                # Instant fill - use \kf (instant fill)
                tag = "\\kf"
            else:
                # Gradual fill - use \k (standard karaoke)
                tag = "\\k"
            
            k_text = ""
            
            if line.tokens:
                # Word-level timing
                current_time = line.start_time
                
                # Handle initial gap
                if line.tokens[0].start_time > current_time:
                    gap = line.tokens[0].start_time - current_time
                    k_text += f"{{{tag}{max(0, int(gap*100))}}}" 
                    current_time += gap
                    
                for token in line.tokens:
                    duration = token.end_time - token.start_time
                    k_val = int(duration * 100)
                    k_text += f"{{{tag}{k_val}}}{token.text} "
            else:
                # Line-level fallback
                duration = line.end_time - line.start_time
                k_val = int(duration * 100)
                k_text = f"{{{tag}{k_val}}}{line.text}"
            
            # --- Global Animation Effects ---
            anim_tags = ""
            
            if self.animation == "Fade In":
                # Fade in effect
                anim_tags = "{\\fad(300,300)}"
            elif self.animation == "Glow Pulse":
                # Scale pulse effect (zoom in then back)
                anim_tags = "{\\fscx90\\fscy90\\t(0,300,\\fscx110\\fscy110)\\t(300,600,\\fscx100\\fscy100)}"
            elif self.animation in ["Linear Wipe", "Bouncing Ball"]:
                # Standard karaoke wipe (already handled by \k tag)
                anim_tags = ""
            
            final_text = anim_tags + k_text

            # --- Select Style Name ---
            style_name = "Default"
            if self.style == "Neon Gold":
                style_name = "NeonSharp"
            elif self.style == "Classic Blue":
                style_name = "Classic"
            elif self.style == "Clean White":
                style_name = "Clean"
                 
            # --- Generate Event Lines ---
            if self.style == "Neon Gold":
                # Neon style uses double layer (blur + sharp)
                glow_line = f"Dialogue: 0,{start_fmt},{end_fmt},NeonBlur,,0,0,0,,{{\\blur15}}{final_text}"
                content.append(glow_line)
                sharp_line = f"Dialogue: 1,{start_fmt},{end_fmt},NeonSharp,,0,0,0,,{{\\blur1}}{final_text}"
                content.append(sharp_line)
            else:
                # Single layer for other styles
                line_str = f"Dialogue: 0,{start_fmt},{end_fmt},{style_name},,0,0,0,,{final_text}"
                content.append(line_str)
            
        return "\n".join(content)

    def _format_time(self, seconds: float) -> str:
        # H:MM:SS.cs
        h = int(seconds // 3600)
        m = int((seconds % 3600) // 60)
        s = int(seconds % 60)
        cs = int((seconds % 1) * 100)
        return f"{h}:{m:02}:{s:02}.{cs:02}"
