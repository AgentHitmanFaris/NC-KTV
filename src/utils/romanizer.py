"""
Romanization utility for NC-KTV
Converts non-Latin scripts (Korean, Japanese, Hindi) to romanized form
"""

import re
from typing import Optional


class Romanizer:
    """Romanization engine supporting multiple languages"""
    
    def __init__(self):
        """Initialize romanizer with lazy loading of libraries"""
        self._kakasi = None  # Japanese
        self._korean_romanizer = None  # Korean
        self._indic_transliterator = None  # Hindi/Indian languages
        
    def romanize(self, text: str, language: str = 'auto') -> str:
        """
        Romanize text based on language
        
        Args:
            text: Text to romanize
            language: Language code ('auto', 'korean', 'japanese', 'hindi', etc.)
            
        Returns:
            Romanized text
        """
        if not text or not text.strip():
            return text
            
        # Auto-detect language if needed
        if language == 'auto':
            language = self._detect_language(text)
            
        if language == 'korean':
            return self._romanize_korean(text)
        elif language == 'japanese':
            return self._romanize_japanese(text)
        elif language in ['hindi', 'tamil', 'bengali', 'telugu', 'marathi']:
            return self._romanize_indic(text)
        else:
            # No romanization needed or unsupported
            return text
            
    def _detect_language(self, text: str) -> str:
        """Auto-detect language from script"""
        # Check for Hangul (Korean)
        if re.search(r'[\uAC00-\uD7AF]', text):
            return 'korean'
            
        # Check for Hiragana/Katakana (Japanese)
        if re.search(r'[\u3040-\u309F\u30A0-\u30FF]', text):
            return 'japanese'
            
        # Check for Devanagari (Hindi and others)
        if re.search(r'[\u0900-\u097F]', text):
            return 'hindi'
            
        # Check for Tamil
        if re.search(r'[\u0B80-\u0BFF]', text):
            return 'tamil'
            
        return 'unknown'
        
    def _romanize_korean(self, text: str) -> str:
        """Romanize Korean (Hangul) to Latin"""
        try:
            if self._korean_romanizer is None:
                from korean_romanizer import Romanizer as KRomanizer
                self._korean_romanizer = KRomanizer(text)
            else:
                self._korean_romanizer = self._korean_romanizer.__class__(text)
            
            result = self._korean_romanizer.romanize()
            print(f"[Romanizer] Korean: '{text}' -> '{result}'")
            return result
        except ImportError as e:
            print(f"[Romanizer] korean_romanizer not installed: {e}")
            # Fallback: Try alternative library
            try:
                from hangul_romanize import Transliter
                transliter = Transliter()
                result = transliter.translit(text)
                print(f"[Romanizer] Korean (alt): '{text}' -> '{result}'")
                return result
            except ImportError as e2:
                print(f"[Romanizer] hangul_romanize not installed: {e2}")
                # Final fallback: basic placeholder
                result = self._fallback_romanize_korean(text)
                print(f"[Romanizer] Korean (fallback): '{text}' -> '{result}'")
                return result
                
    def _fallback_romanize_korean(self, text: str) -> str:
        """Simple fallback for Korean romanization without external libraries"""
        # Simple placeholder to show it's working
        # You can install proper libraries later for accurate romanization
        return f"[KO: {text}]"  # Temporary indicator that detection works
        
    def _romanize_japanese(self, text: str) -> str:
        """Romanize Japanese (Hiragana/Katakana/Kanji) to Romaji"""
        try:
            if self._kakasi is None:
                import pykakasi
                self._kakasi = pykakasi.kakasi()
                
            result = self._kakasi.convert(text)
            # Extract romaji from result
            romanized = ' '.join([item['hepburn'] for item in result])
            print(f"[Romanizer] Japanese: '{text}' -> '{romanized}'")
            return romanized
        except ImportError as e:
            print(f"[Romanizer] pykakasi not installed: {e}")
            # Fallback
            result = self._fallback_romanize_japanese(text)
            print(f"[Romanizer] Japanese (fallback): '{text}' -> '{result}'")
            return result
            
    def _fallback_romanize_japanese(self, text: str) -> str:
        """Simple fallback for Japanese without pykakasi"""
        # Basic Hiragana romanization table
        hiragana_map = {
            'あ': 'a', 'い': 'i', 'う': 'u', 'え': 'e', 'お': 'o',
            'か': 'ka', 'き': 'ki', 'く': 'ku', 'け': 'ke', 'こ': 'ko',
            'さ': 'sa', 'し': 'shi', 'す': 'su', 'せ': 'se', 'そ': 'so',
            'た': 'ta', 'ち': 'chi', 'つ': 'tsu', 'て': 'te', 'と': 'to',
            'な': 'na', 'に': 'ni', 'ぬ': 'nu', 'ね': 'ne', 'の': 'no',
            'は': 'ha', 'ひ': 'hi', 'ふ': 'fu', 'へ': 'he', 'ほ': 'ho',
            'ま': 'ma', 'み': 'mi', 'む': 'mu', 'め': 'me', 'も': 'mo',
            'や': 'ya', 'ゆ': 'yu', 'よ': 'yo',
            'ら': 'ra', 'り': 'ri', 'る': 'ru', 'れ': 're', 'ろ': 'ro',
            'わ': 'wa', 'を': 'wo', 'ん': 'n',
            'が': 'ga', 'ぎ': 'gi', 'ぐ': 'gu', 'げ': 'ge', 'ご': 'go',
            'ざ': 'za', 'じ': 'ji', 'ず': 'zu', 'ぜ': 'ze', 'ぞ': 'zo',
            'だ': 'da', 'ぢ': 'ji', 'づ': 'zu', 'で': 'de', 'ど': 'do',
            'ば': 'ba', 'び': 'bi', 'ぶ': 'bu', 'べ': 'be', 'ぼ': 'bo',
            'ぱ': 'pa', 'ぴ': 'pi', 'ぷ': 'pu', 'ぺ': 'pe', 'ぽ': 'po',
            # Katakana
            'ア': 'a', 'イ': 'i', 'ウ': 'u', 'エ': 'e', 'オ': 'o',
            'カ': 'ka', 'キ': 'ki', 'ク': 'ku', 'ケ': 'ke', 'コ': 'ko',
            'サ': 'sa', 'シ': 'shi', 'ス': 'su', 'セ': 'se', 'ソ': 'so',
            'タ': 'ta', 'チ': 'chi', 'ツ': 'tsu', 'テ': 'te', 'ト': 'to',
            'ナ': 'na', 'ニ': 'ni', 'ヌ': 'nu', 'ネ': 'ne', 'ノ': 'no',
            'ハ': 'ha', 'ヒ': 'hi', 'フ': 'fu', 'ヘ': 'he', 'ホ': 'ho',
            'マ': 'ma', 'ミ': 'mi', 'ム': 'mu', 'メ': 'me', 'モ': 'mo',
            'ヤ': 'ya', 'ユ': 'yu', 'ヨ': 'yo',
            'ラ': 'ra', 'リ': 'ri', 'ル': 'ru', 'レ': 're', 'ロ': 'ro',
            'ワ': 'wa', 'ヲ': 'wo', 'ン': 'n',
        }
        
        result = []
        for char in text:
            result.append(hiragana_map.get(char, char))
        return ''.join(result)
        
    def _romanize_indic(self, text: str) -> str:
        """Romanize Indic scripts (Hindi, Tamil, etc.) to Latin"""
        try:
            if self._indic_transliterator is None:
                from indic_transliteration import sanscript
                from indic_transliteration.sanscript import transliterate
                self._indic_transliterator = transliterate
                
            # Detect script and transliterate
            # Devanagari (Hindi) -> ITRANS/ISO
            result = self._indic_transliterator(sanscript.DEVANAGARI, sanscript.ITRANS, text)
            print(f"[Romanizer] Hindi: '{text}' -> '{result}'")
            return result
        except ImportError as e:
            print(f"[Romanizer] indic_transliteration not installed: {e}")
            # Fallback without library - use placeholder
            result = f"[HI: {text}]"
            print(f"[Romanizer] Hindi (fallback): '{text}' -> '{result}'")
            return result


# Global singleton instance
_romanizer_instance = None


def get_romanizer() -> Romanizer:
    """Get global romanizer instance"""
    global _romanizer_instance
    if _romanizer_instance is None:
        _romanizer_instance = Romanizer()
    return _romanizer_instance
