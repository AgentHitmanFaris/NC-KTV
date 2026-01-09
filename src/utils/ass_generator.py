"""
ASS (Advanced Substation Alpha) Generator for Karaoke
"""
from core.lyrics import LyricsData

class ASSGenerator:
    def __init__(self, lyrics_data: LyricsData, style: str = "Neon Gold", animation: str = "Standard (Wipe)", custom_style=None, global_offset: float = 0.0):
        self.lyrics = lyrics_data
        self.style = style
        self.animation = animation
        self.custom_style = custom_style or {}
        self.global_offset = global_offset
        
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

    def _to_ass_color(self, color_obj, alpha=0):
        """Helper for ASS color: &HAABBGGRR"""
        if isinstance(color_obj, str):
            # Assume hex string but ASS needs &H
            return "&H00FFFFFF" 
        # If it's a QColor or similar tuple
        try:
            # Assuming RGBA or close
            # We need to flip to BGR
            return f"&H{alpha:02X}{color_obj.blue():02X}{color_obj.green():02X}{color_obj.red():02X}"
        except:
            return "&H00FFFFFF"

    def _generate_styles(self):
        """Define Styles based on selection"""
        base_header = """[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding"""
        
        # Helper for ASS color: &HAABBGGRR
        # (Moved to class method _to_ass_color)

        if self.style == "Match Preview" and self.custom_style:
            # Generate style from preview settings
            # We use 'Preview' as the style name
            primary = self._to_ass_color(self.custom_style.get('active_color'), 0)
            secondary = self._to_ass_color(self.custom_style.get('inactive_color'), 0) # Secondary is often the "wait" color in ASS karaoke
            outline = self._to_ass_color(self.custom_style.get('outline_color'), 0)
            
            # Simple styles
            return base_header + f"""
Style: Preview,Arial,60,{primary},{secondary},{outline},&H00000000,1,0,0,0,100,100,0,0,1,2,0,2,20,20,50,1
"""

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
        
        lines = self.lyrics.lines
        if not lines:
            return "\n".join(content)

        # --- Countdown Logic ---
        # If the first line starts after 4 seconds (plus global offset), add a countdown
        first_start = max(0, lines[0].start_time + self.global_offset)
        
        if first_start > 4.0:
            # 4 beats/seconds of countdown
            # "3" -> "2" -> "1" -> "GO"
            # Each lasts ~0.8s
            
            # Timings relative to first lyric start
            # 3: T-4
            # 2: T-3
            # 1: T-2
            # GO: T-1
            
            cd_steps = [
                (first_start - 4.0, "3"),
                (first_start - 3.0, "2"),
                (first_start - 2.0, "1"),
                (first_start - 1.0, "GO")
            ]
            
            # Determine style for countdown
            # Use 'Default' or 'Preview' depending on mode, but force centered position
            cd_style = "Default"
            if self.style == "Match Preview":
                cd_style = "Preview"
            elif self.style == "Neon Gold":
                cd_style = "NeonSharp"
            elif self.style == "Classic Blue":
                cd_style = "Classic"
            elif self.style == "Clean White":
                cd_style = "Clean"

            for t_start, text in cd_steps:
                if t_start < 0: continue # Skip if negative time
                
                t_end = t_start + 0.8 # Short duration
                
                s_fmt = self._format_time(t_start)
                e_fmt = self._format_time(t_end)
                
                # Force center screen for countdown
                # {\pos(960,540)} is center of 1920x1080
                # Use \fade(255,0,255, t1, t2, t3, t4) or just simple \fad(100,100)
                formatted_line = f"Dialogue: 0,{s_fmt},{e_fmt},{cd_style},,0,0,0,,{{\\pos(960,540)}}{{\\fad(100,100)}}{text}"
                content.append(formatted_line)

        for i, line in enumerate(lines):
            # Apply global offset
            start_t = max(0, line.start_time + self.global_offset)
            end_t = max(0, line.end_time + self.global_offset)
            
            start_fmt = self._format_time(start_t)
            end_fmt = self._format_time(end_t)
            
            # Lookahead for next line
            next_line = lines[i+1] if i + 1 < len(lines) else None
            
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
                # Line-level fallback: Simulate word timing by splitting text
                words = line.text.split(' ')
                if not words:
                    continue
                    
                total_duration = line.end_time - line.start_time
                word_duration = total_duration / len(words)
                k_val = int(word_duration * 100)
                
                for i, word in enumerate(words):
                    space = " " if i < len(words) - 1 else ""
                    k_text += f"{{{tag}{k_val}}}{word}{space}"
            
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
            if self.style == "Match Preview":
                style_name = "Preview"
            elif self.style == "Neon Gold":
                style_name = "NeonSharp"
            elif self.style == "Classic Blue":
                style_name = "Classic"
            elif self.style == "Clean White":
                style_name = "Clean"
                 
            # --- Generate Event Lines ---
            
            # Position Overrides for "Match Preview"
            # User reported Center (540) is "too high". Moving to bottom.
            # Active Line: y=900
            # Next Line: y=1020
            pos_tag = ""
            next_pos_tag = ""
            
            if self.style == "Match Preview":
                pos_tag = "{\\pos(960,900)}"
                # Only show next line if enabled/available
                if next_line:
                    # Inactive color (grayish), static
                    # Use secondary style or override
                    pass
            
            if self.style == "Neon Gold":
                # Neon style uses double layer (blur + sharp)
                glow_line = f"Dialogue: 0,{start_fmt},{end_fmt},NeonBlur,,0,0,0,,{{\\blur15}}{final_text}"
                content.append(glow_line)
                sharp_line = f"Dialogue: 1,{start_fmt},{end_fmt},NeonSharp,,0,0,0,,{{\\blur1}}{final_text}"
                content.append(sharp_line)
            else:
                # Single layer for other styles
                # Apply pos_tag if Match Preview
                text_with_pos = f"{pos_tag}{final_text}"
                line_str = f"Dialogue: 0,{start_fmt},{end_fmt},{style_name},,0,0,0,,{text_with_pos}"
                content.append(line_str)
                
            # --- Generate Next Line Preview (Match Preview Only for now) ---
            if self.style == "Match Preview" and next_line:
                # Next line text (plain, no k tags)
                next_text = next_line.text
                next_pos = "{\\pos(960,1020)}"
                
                # Use inactive color override
                color_tag = ""
                if self.custom_style and 'inactive_color' in self.custom_style:
                    # Ass \c&H...& sets primary fill color
                    inactive_hex = self._to_ass_color(self.custom_style.get('inactive_color'), 0)
                    # Strip the &H prefix for \c tag if needed? No, ASS \c expects &H...& or &H...
                    # Actually standard tag is \c&HBBGGRR&
                    color_tag = f"{{\\c{inactive_hex}&}}"
                
                # Check if next_line needs time constraints
                # It should appear during the CURRENT line's duration
                
                # We add \alpha&H80& for transparency, and explicit color tag
                preview_line = f"Dialogue: 0,{start_fmt},{end_fmt},{style_name},,0,0,0,,{next_pos}{color_tag}{{\\alpha&H80&}}{next_text}"
                content.append(preview_line)
            
        return "\n".join(content)

    def _format_time(self, seconds: float) -> str:
        # H:MM:SS.cs
        h = int(seconds // 3600)
        m = int((seconds % 3600) // 60)
        s = int(seconds % 60)
        cs = int((seconds % 1) * 100)
        return f"{h}:{m:02}:{s:02}.{cs:02}"
