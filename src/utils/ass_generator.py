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
            
            # --- Karaoke Wipe Tag Construction ---
            k_text = ""
            
            # Decide tag type: \k (fill), \kf (fill), \ko (outline)
            # Typewriter uses standard \k but style makes it appear
            tag = "\\k" 
            
            if line.tokens:
                # Word-level
                current_time = line.start_time
                
                # Handle initial gap
                if line.tokens[0].start_time > current_time:
                    gap = line.tokens[0].start_time - current_time
                    k_text += f"{{{tag}{max(0, int(gap*100))}}}" 
                    current_time += gap
                    
                for token in line.tokens:
                    duration = token.end_time - token.start_time
                    k_val = int(duration * 100)
                    
                    # Effect modifiers per word
                    effect_mod = ""
                    
                    if self.animation == "Scale Pulse":
                        # Pulse up then down during the word
                        # \t(t1,t2,accel,tags)
                        # We want it to pop up at start of its time
                        # Start of word event is relative 0 for the \t if inside the K tag? No, \t is relative to line start.
                        # Actually \t is absolute time within the line event.
                        # Complex logic needed for per-word \t since we are concatting.
                        # Simplification: Just scale up slightly for the duration?
                        pass 
                    
                    k_text += f"{{{tag}{k_val}}}{token.text} "
            else:
                # Line-level fallback
                duration = line.end_time - line.start_time
                k_val = int(duration * 100)
                k_text = f"{{{tag}{k_val}}}{line.text}"
            
            # --- Global Animation Tags ---
            anim_tags = ""
            if self.animation == "Fade In":
                anim_tags = "{\\fad(300,300)}"
            elif self.animation == "Zoom In": # "Zoom In" in Dialog matches "Zoom In"
                anim_tags = "{\\fscx90\\fscy90\\t(0,200,\\fscx100\\fscy100)}"
            elif self.animation == "Slide Up":
                anim_tags = "{\\move(960,1080,960,1030)}"
            elif self.animation == "Rainbow":
                # Cycle colors: Red->Yellow->Green->Cyan->Blue->Magenta->Red
                # 4s cycle
                anim_tags = "{\\t(0,1000,\\1c&H00FFFF)\\t(1000,2000,\\1c&H00FF00)\\t(2000,3000,\\1c&HFF0000)\\t(3000,4000,\\1c&H0000FF)}"
            elif self.animation == "Scale Pulse":
                # Static pulse for whole line for now, per-word is hard in ASS concat
                anim_tags = "" 

            final_text = anim_tags + k_text

            # --- Event Generation based on Style ---
            style_name = "Default"
            if self.animation == "Typewriter":
                 style_name = "Typewriter"
            elif self.style == "Neon Gold":
                 style_name = "NeonSharp" # We handle layer 2 below
            elif self.style == "Classic Blue":
                 style_name = "Classic"
            elif self.style == "Clean White":
                 style_name = "Clean"
                 
            # Double Layer Effects
            if self.style == "Neon Gold" and self.animation != "Typewriter":
                # Blur Layer
                glow_line = f"Dialogue: 0,{start_fmt},{end_fmt},NeonBlur,,0,0,0,,{{\\blur15}}{final_text}"
                content.append(glow_line)
                # Sharp Layer
                sharp_line = f"Dialogue: 1,{start_fmt},{end_fmt},NeonSharp,,0,0,0,,{{\\blur1}}{final_text}"
                content.append(sharp_line)
            else:
                # Single Layer
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
