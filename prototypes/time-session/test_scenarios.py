# PROTOTYPE - NOT FOR PRODUCTION
# Question: 7개의 실제 시나리오에서 분류기가 올바르게 작동하는가?
# Date: 2026-04-16

from datetime import datetime, timedelta
from classifier import (
    SessionRecord,
    classify,
    BetweenSessionClass,
    InSessionClass,
    Action,
)


BASE = datetime(2026, 4, 16, 8, 0, 0)  # 2026-04-16 08:00:00 UTC
SESSION_A = "a1b2c3d4"
SESSION_B = "e5f6g7h8"


def pretty(title: str, result, expected_class, expected_action):
    cls = (result.between_class or result.in_class).value
    pass_cls = cls == expected_class
    pass_act = result.action == expected_action
    status = "✅ PASS" if (pass_cls and pass_act) else "❌ FAIL"
    print(f"\n{status} — {title}")
    print(f"  Classification : {cls}")
    print(f"  Action         : {result.action.value}")
    print(f"  Wall delta     : {result.wall_delta}")
    print(f"  Monotonic delta: {result.monotonic_delta}")
    if result.notes:
        print(f"  Notes          : {result.notes}")
    if not pass_cls:
        print(f"  🔴 Expected class  : {expected_class}")
    if not pass_act:
        print(f"  🔴 Expected action : {expected_action.value}")
    return pass_cls and pass_act


def run_all():
    print("=" * 70)
    print("TIME & SESSION CLASSIFIER — 7 SCENARIO TEST HARNESS")
    print("Moss Baby, UE 5.6 prototype spike")
    print("=" * 70)

    results = []

    # ── Scenario 1: Normal Close & Reopen (between-session)
    prev = SessionRecord(
        session_uuid=SESSION_A,
        last_wall_utc=BASE,
        last_monotonic_sec=120.0,  # 2 min into session
        day_index=3,
    )
    # App closed, reopened 12 hours later → new session_uuid, monotonic reset
    result = classify(
        prev,
        current_wall_utc=BASE + timedelta(hours=12),
        current_monotonic_sec=1.2,  # new session, small monotonic
        current_session_uuid=SESSION_B,
    )
    results.append(
        pretty("1. Normal Close & Reopen (12h gap)",
               result, "ACCEPTED_GAP", Action.ADVANCE_SILENT)
    )

    # ── Scenario 2: Laptop Sleep (in-session) — monotonic stalls
    prev = SessionRecord(
        session_uuid=SESSION_A,
        last_wall_utc=BASE,
        last_monotonic_sec=600.0,
        day_index=3,
    )
    # System slept 3h; wall advanced but monotonic barely moved
    result = classify(
        prev,
        current_wall_utc=BASE + timedelta(hours=3),
        current_monotonic_sec=600.5,   # 0.5s since sleep
        current_session_uuid=SESSION_A,  # SAME session
    )
    results.append(
        pretty("2. Laptop Sleep 3h (in-session, monotonic stalls)",
               result, "SUSPEND_RESUME", Action.ADVANCE_SILENT)
    )

    # ── Scenario 3: Forward Tamper (between-session) — clock set +5 days
    prev = SessionRecord(
        session_uuid=SESSION_A,
        last_wall_utc=BASE,
        last_monotonic_sec=300.0,
        day_index=3,
    )
    # App closed ~10 seconds, but system clock set +5 days
    result = classify(
        prev,
        current_wall_utc=BASE + timedelta(days=5),
        current_monotonic_sec=0.8,
        current_session_uuid=SESSION_B,
    )
    # Per user decision D: accept + narrative (gap > 24h)
    results.append(
        pretty("3. Forward Tamper +5d (between-session, accepted with narrative)",
               result, "ACCEPTED_GAP", Action.ADVANCE_WITH_NARRATIVE)
    )

    # ── Scenario 4: Backward Tamper (between-session) — clock set -2 days
    prev = SessionRecord(
        session_uuid=SESSION_A,
        last_wall_utc=BASE,
        last_monotonic_sec=300.0,
        day_index=3,
    )
    result = classify(
        prev,
        current_wall_utc=BASE - timedelta(days=2),
        current_monotonic_sec=0.8,
        current_session_uuid=SESSION_B,
    )
    results.append(
        pretty("4. Backward Tamper −2d (between-session, reject)",
               result, "BACKWARD_GAP", Action.HOLD_LAST_TIME)
    )

    # ── Scenario 5: NTP Re-sync after sleep (in-session) — +15 seconds
    # NOTE (finding from first run): sub-5s NTP drift classifies as NORMAL_TICK,
    # not MINOR_SHIFT, because it's within the default epsilon. Both lead to
    # ADVANCE_SILENT — the distinction is cosmetic. To test MINOR_SHIFT meaningfully,
    # we use a +15s jump (realistic for post-sleep NTP re-sync).
    prev = SessionRecord(
        session_uuid=SESSION_A,
        last_wall_utc=BASE,
        last_monotonic_sec=600.0,
        day_index=3,
    )
    result = classify(
        prev,
        current_wall_utc=BASE + timedelta(minutes=10, seconds=15),
        current_monotonic_sec=600.0 + 600.0,  # 10 real minutes of monotonic
        current_session_uuid=SESSION_A,
    )
    results.append(
        pretty("5. NTP Re-sync +15s (in-session)",
               result, "MINOR_SHIFT", Action.ADVANCE_SILENT)
    )

    # ── Scenario 6: DST Spring-forward (in-session) — +1 hour
    prev = SessionRecord(
        session_uuid=SESSION_A,
        last_wall_utc=BASE,
        last_monotonic_sec=600.0,
        day_index=3,
    )
    # 30 real minutes passed, but DST jumps wall clock +1h at boundary
    result = classify(
        prev,
        current_wall_utc=BASE + timedelta(hours=1, minutes=30),
        current_monotonic_sec=600.0 + 1800.0,  # 30 real minutes
        current_session_uuid=SESSION_A,
    )
    # Expect MINOR_SHIFT (1h discrepancy within 90min tolerance)
    results.append(
        pretty("6. DST Spring-forward +1h (in-session)",
               result, "MINOR_SHIFT", Action.ADVANCE_SILENT)
    )

    # ── Scenario 7: Long Gap > 21 days (between-session) — player away 30 days
    prev = SessionRecord(
        session_uuid=SESSION_A,
        last_wall_utc=BASE,
        last_monotonic_sec=300.0,
        day_index=5,
    )
    result = classify(
        prev,
        current_wall_utc=BASE + timedelta(days=30),
        current_monotonic_sec=0.5,
        current_session_uuid=SESSION_B,
    )
    results.append(
        pretty("7. Long Gap 30d (between-session, prompt player choice)",
               result, "LONG_GAP", Action.PROMPT_LONG_GAP_CHOICE)
    )

    # ── Bonus edge cases
    print("\n" + "─" * 70)
    print("BONUS EDGE CASES")
    print("─" * 70)

    # E1: Exactly 21 days (boundary)
    prev = SessionRecord(SESSION_A, BASE, 300.0, 1)
    result = classify(
        prev,
        current_wall_utc=BASE + timedelta(days=21),
        current_monotonic_sec=0.5,
        current_session_uuid=SESSION_B,
    )
    results.append(
        pretty("E1. Exactly 21-day gap (boundary, should accept not long)",
               result, "ACCEPTED_GAP", Action.ADVANCE_WITH_NARRATIVE)
    )

    # E2: 21 days + 1 second (just over)
    prev = SessionRecord(SESSION_A, BASE, 300.0, 1)
    result = classify(
        prev,
        current_wall_utc=BASE + timedelta(days=21, seconds=1),
        current_monotonic_sec=0.5,
        current_session_uuid=SESSION_B,
    )
    results.append(
        pretty("E2. 21 days + 1s (just over threshold → LONG_GAP)",
               result, "LONG_GAP", Action.PROMPT_LONG_GAP_CHOICE)
    )

    # E3: First run
    result = classify(
        None,
        current_wall_utc=BASE,
        current_monotonic_sec=0.3,
        current_session_uuid=SESSION_A,
    )
    results.append(
        pretty("E3. First run (no prev session)",
               result, "FIRST_RUN", Action.START_DAY_ONE)
    )

    # E4: In-session large discrepancy (user changed clock while app running)
    prev = SessionRecord(SESSION_A, BASE, 600.0, 3)
    result = classify(
        prev,
        current_wall_utc=BASE + timedelta(hours=5),
        current_monotonic_sec=600.0 + 120.0,  # 2 min of monotonic, 5h of wall
        current_session_uuid=SESSION_A,
    )
    results.append(
        pretty("E4. In-session 5h jump (discrepancy, log only)",
               result, "IN_SESSION_DISCREPANCY", Action.LOG_ONLY)
    )

    # E5: Short close/reopen (1 min, same day)
    prev = SessionRecord(SESSION_A, BASE, 120.0, 3)
    result = classify(
        prev,
        current_wall_utc=BASE + timedelta(minutes=1),
        current_monotonic_sec=0.3,
        current_session_uuid=SESSION_B,
    )
    # Should be ACCEPTED_GAP, silent (no narrative, <24h)
    results.append(
        pretty("E5. Quick restart (1 min gap, silent)",
               result, "ACCEPTED_GAP", Action.ADVANCE_SILENT)
    )

    # ── Summary
    print("\n" + "=" * 70)
    passed = sum(1 for r in results if r)
    total = len(results)
    print(f"RESULT: {passed}/{total} scenarios passed")
    print("=" * 70)
    return passed == total


if __name__ == "__main__":
    import sys
    success = run_all()
    sys.exit(0 if success else 1)
