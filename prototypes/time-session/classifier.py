# PROTOTYPE - NOT FOR PRODUCTION
# Question: 이중 시계(WallClock + Monotonic) 접근법이 21일 실시간 진행의
#           시스템 시간 조작·suspend/resume·NTP·DST·장기 갭을 정확히 분류하는가?
# Date: 2026-04-16
#
# UE 5.6 대응: WallClock = FDateTime::UtcNow() / Monotonic = FPlatformTime::Seconds()
# 이 Python은 알고리즘 검증용. 실제 구현은 UE C++로 재작성 (프로토타입 코드 재사용 금지).

from dataclasses import dataclass, field
from datetime import datetime, timedelta
from enum import Enum
from typing import Optional
import uuid


# ─── Constants (Moss Baby specific) ─────────────────────────────────

GAME_DURATION_DAYS = 21
LONG_GAP_THRESHOLD = timedelta(days=GAME_DURATION_DAYS)  # LONG_GAP 기준선
IN_SESSION_DISCREPANCY_TOLERANCE = timedelta(minutes=90)  # NTP/DST 흡수 한계
DEFAULT_EPSILON = timedelta(seconds=5)  # NORMAL_TICK 허용 오차


# ─── Classification Types ───────────────────────────────────────────

class BetweenSessionClass(Enum):
    FIRST_RUN = "FIRST_RUN"
    BACKWARD_GAP = "BACKWARD_GAP"       # Backward tamper
    ACCEPTED_GAP = "ACCEPTED_GAP"       # Normal/forward-accepted
    LONG_GAP = "LONG_GAP"               # > 21 days → player choice prompt


class InSessionClass(Enum):
    NORMAL_TICK = "NORMAL_TICK"
    SUSPEND_RESUME = "SUSPEND_RESUME"   # wall advanced, monotonic stalled
    MINOR_SHIFT = "MINOR_SHIFT"         # NTP / DST 흡수
    IN_SESSION_DISCREPANCY = "IN_SESSION_DISCREPANCY"  # 대규모 불일치 → 로그만


class Action(Enum):
    START_DAY_ONE = "START_DAY_ONE"
    HOLD_LAST_TIME = "HOLD_LAST_TIME"
    ADVANCE_SILENT = "ADVANCE_SILENT"
    ADVANCE_WITH_NARRATIVE = "ADVANCE_WITH_NARRATIVE"
    PROMPT_LONG_GAP_CHOICE = "PROMPT_LONG_GAP_CHOICE"
    LOG_ONLY = "LOG_ONLY"


# ─── Session Record (persisted between runs) ────────────────────────

@dataclass
class SessionRecord:
    session_uuid: str
    last_wall_utc: datetime
    last_monotonic_sec: float       # FPlatformTime::Seconds() at last save
    day_index: int                  # 1..21
    last_save_reason: str = "init"


# ─── Classifier ─────────────────────────────────────────────────────

@dataclass
class ClassificationResult:
    is_between_session: bool
    between_class: Optional[BetweenSessionClass] = None
    in_class: Optional[InSessionClass] = None
    action: Action = Action.ADVANCE_SILENT
    wall_delta: timedelta = timedelta(0)
    monotonic_delta: timedelta = timedelta(0)
    discrepancy: timedelta = timedelta(0)
    notes: str = ""


def classify(
    prev: Optional[SessionRecord],
    current_wall_utc: datetime,
    current_monotonic_sec: float,
    current_session_uuid: str,
) -> ClassificationResult:
    """
    Dual-clock classifier.

    - If prev is None → FIRST_RUN
    - If prev.session_uuid != current_session_uuid → between-session (app restarted)
    - Otherwise → in-session
    """
    if prev is None:
        return ClassificationResult(
            is_between_session=True,
            between_class=BetweenSessionClass.FIRST_RUN,
            action=Action.START_DAY_ONE,
            notes="No prior session.",
        )

    wall_delta = current_wall_utc - prev.last_wall_utc
    mono_delta_sec = current_monotonic_sec - prev.last_monotonic_sec
    mono_delta = timedelta(seconds=mono_delta_sec) if mono_delta_sec >= 0 else timedelta(seconds=0)

    # Between-session path (session_uuid changed → app restart)
    if prev.session_uuid != current_session_uuid:
        if wall_delta < timedelta(0):
            return ClassificationResult(
                is_between_session=True,
                between_class=BetweenSessionClass.BACKWARD_GAP,
                action=Action.HOLD_LAST_TIME,
                wall_delta=wall_delta,
                monotonic_delta=mono_delta,
                notes="Wall clock moved backward across session boundary; hold last known time.",
            )
        if wall_delta > LONG_GAP_THRESHOLD:
            return ClassificationResult(
                is_between_session=True,
                between_class=BetweenSessionClass.LONG_GAP,
                action=Action.PROMPT_LONG_GAP_CHOICE,
                wall_delta=wall_delta,
                monotonic_delta=mono_delta,
                notes=f"Gap of {wall_delta} exceeds 21-day arc; prompt player for continue/farewell choice.",
            )
        # ACCEPTED_GAP: per user decision D, accept all forward advances.
        # Narrative line fires if gap > 24h, per Moss Baby Quiet Presence tone.
        narrative = wall_delta > timedelta(hours=24)
        return ClassificationResult(
            is_between_session=True,
            between_class=BetweenSessionClass.ACCEPTED_GAP,
            action=Action.ADVANCE_WITH_NARRATIVE if narrative else Action.ADVANCE_SILENT,
            wall_delta=wall_delta,
            monotonic_delta=mono_delta,
            notes=f"Accept gap of {wall_delta}. Narrative={narrative}.",
        )

    # In-session path (same session uuid)
    discrepancy = wall_delta - mono_delta
    abs_disc = abs(discrepancy)

    if abs_disc <= DEFAULT_EPSILON:
        return ClassificationResult(
            is_between_session=False,
            in_class=InSessionClass.NORMAL_TICK,
            action=Action.ADVANCE_SILENT,
            wall_delta=wall_delta,
            monotonic_delta=mono_delta,
            discrepancy=discrepancy,
        )

    # Suspend/resume: wall advanced significantly while monotonic stalled (small)
    # Heuristic: mono_delta < 5s but wall_delta is notably larger (> 60s)
    if mono_delta < timedelta(seconds=5) and wall_delta > timedelta(seconds=60):
        return ClassificationResult(
            is_between_session=False,
            in_class=InSessionClass.SUSPEND_RESUME,
            action=Action.ADVANCE_SILENT,
            wall_delta=wall_delta,
            monotonic_delta=mono_delta,
            discrepancy=discrepancy,
            notes="Monotonic stalled while wall advanced — likely system sleep.",
        )

    # Minor shift: NTP sync or DST boundary — absorb silently within tolerance
    if abs_disc <= IN_SESSION_DISCREPANCY_TOLERANCE:
        return ClassificationResult(
            is_between_session=False,
            in_class=InSessionClass.MINOR_SHIFT,
            action=Action.ADVANCE_SILENT,
            wall_delta=wall_delta,
            monotonic_delta=mono_delta,
            discrepancy=discrepancy,
            notes=f"Clock skew {discrepancy} within tolerance (NTP/DST).",
        )

    # Large in-session discrepancy: log only, advance by monotonic (trust the clock that can't be user-set)
    return ClassificationResult(
        is_between_session=False,
        in_class=InSessionClass.IN_SESSION_DISCREPANCY,
        action=Action.LOG_ONLY,
        wall_delta=wall_delta,
        monotonic_delta=mono_delta,
        discrepancy=discrepancy,
        notes=f"Large in-session clock skew {discrepancy}; advance by monotonic only.",
    )
