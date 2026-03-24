"""
Hallucination detection and filtering for Whisper/WhisperX transcripts.
Ported and adapted from Nightingale (https://github.com/rzru/nightingale).

Whisper models frequently hallucinate attribution text, promotional phrases,
and other non-lyric content — especially during silent or low-energy audio
regions. This module provides utilities to detect and remove such artifacts.
"""

import re


# ── Known Hallucination Patterns ─────────────────────────────────────────────

BANNED_WORDS = {
    "dimatorzok", "dimatorsok", "dima_torzok",
    "amara.org",
}

HALLUCINATION_PHRASES = [
    # English
    "thanks for watching",
    "please subscribe",
    "like and subscribe",
    "song lyrics",
    "all rights reserved",
    "subtitles by",
    "captions by",
    "thank you for watching",
    "don't forget to subscribe",
    "hit the bell",
    "click the link",
    # Russian (common Whisper hallucinations)
    "продолжение следует",
    "подписывайтесь на канал",
    "редактор субтитров",
    "корректор субтитров",
    "субтитры подогнал",
    "субтитры подогнала",
    "субтитры сделал",
    "субтитры сделала",
    # Other
    "satsang with mooji",
]

ATTRIBUTION_WORDS = {
    # Russian
    "субтитры", "субтитр", "подписи", "титры",
    "сделал", "сделала", "сделали",
    "создал", "создала", "создали",
    "делал", "делала", "делали",
    "создавал", "создавала", "создавали",
    "подготовил", "подготовила", "подготовили",
    "редактировал", "редактировала", "редактировали",
    "выполнил", "выполнила", "выполнили",
    "подогнал", "подогнала", "подогнали",
    "перевёл", "перевела", "перевели",
    "редактор", "корректор", "переводчик",
    # English
    "subtitles", "subtitle", "captions", "caption",
    "transcribed", "transcript", "transcription",
    "editor", "translator", "proofreader",
}


def is_hallucination(segment: dict) -> bool:
    """Check if a transcribed segment is a known hallucination or attribution.
    
    Args:
        segment: A dict with at least a 'text' key.
    
    Returns:
        True if the segment appears to be hallucinated.
    """
    text = segment.get("text", "").strip()
    if not text:
        return True

    text_lower = text.lower()

    # Check for known hallucination phrases
    for phrase in HALLUCINATION_PHRASES:
        if phrase in text_lower:
            return True

    words = text.split()
    if not words:
        return True

    # Check for attribution-heavy segments
    attr_count = 0
    for w in words:
        clean = re.sub(r"[.,!?;:\"']", "", w).lower()
        if clean in BANNED_WORDS or clean in ATTRIBUTION_WORDS:
            attr_count += 1

    # All words are attribution → hallucination
    if attr_count == len(words):
        return True

    # Majority of words are attribution in longer segments
    if len(words) >= 3 and attr_count / len(words) >= 0.5:
        return True

    return False


def filter_hallucinated_segments(segments: list, duration_secs: float = 0.0) -> list:
    """Remove hallucinated segments from a list of transcription segments.
    
    Args:
        segments: List of segment dicts with 'text', 'start', 'end' keys.
        duration_secs: Total audio duration for coverage logging.
    
    Returns:
        Filtered list of segments with hallucinations removed.
    """
    good_segments = []
    hallucinated = []

    for seg in segments:
        if is_hallucination(seg):
            hallucinated.append(seg)
        else:
            good_segments.append(seg)

    if hallucinated:
        import sys
        for seg in hallucinated:
            dur = seg.get("end", 0) - seg.get("start", 0)
            print(
                f"[WhisperX] Discarded hallucination "
                f"[{seg.get('start', 0):.1f}-{seg.get('end', 0):.1f}] "
                f"({dur:.1f}s): {seg.get('text', '')[:80]}",
                file=sys.stderr
            )
        print(
            f"[WhisperX] Kept {len(good_segments)} segments, "
            f"discarded {len(hallucinated)} hallucinations",
            file=sys.stderr
        )

    if duration_secs > 0:
        import sys
        covered = sum(s.get("end", 0) - s.get("start", 0) for s in good_segments)
        print(
            f"[WhisperX] Coverage: {covered:.1f}s / {duration_secs:.1f}s "
            f"({covered / duration_secs * 100:.0f}%)",
            file=sys.stderr
        )

    return good_segments


def remove_hallucinated_words(words: list) -> list:
    """Remove individual hallucinated/attribution words and their neighbors.
    
    Args:
        words: List of word dicts with at least a 'word' key.
    
    Returns:
        Filtered list with hallucinated words removed.
    """
    if not words:
        return words

    to_remove: set = set()

    for i, w in enumerate(words):
        clean = re.sub(r"[.,!?;:\"']", "", w.get("word", "")).lower()
        if clean in BANNED_WORDS:
            to_remove.add(i)
            for j in range(max(0, i - 4), i):
                neighbor = re.sub(r"[.,!?;:\"']", "", words[j].get("word", "")).lower()
                if neighbor in ATTRIBUTION_WORDS:
                    to_remove.add(j)
            for j in range(i + 1, min(len(words), i + 3)):
                neighbor = re.sub(r"[.,!?;:\"']", "", words[j].get("word", "")).lower()
                if neighbor in ATTRIBUTION_WORDS:
                    to_remove.add(j)

    if to_remove:
        import sys
        removed_text = " ".join(words[i].get("word", "") for i in sorted(to_remove))
        print(
            f"[WhisperX] Removed hallucination ({len(to_remove)} words): {removed_text}",
            file=sys.stderr
        )

    return [w for i, w in enumerate(words) if i not in to_remove]
