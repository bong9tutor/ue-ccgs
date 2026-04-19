// Copyright Moss Baby
//
// Story-001: IMossClockSource 인터페이스 + FRealClockSource + FMockClockSource
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
//
// 이 파일은 Time & Session System의 시간 입력 seam을 정의한다.
// 모든 시간 API 호출(FDateTime::UtcNow / FPlatformTime::Seconds)은
// FRealClockSource 내부에서만 허용된다. (ADR-0001 §Required Pattern)

#pragma once
#include "CoreMinimal.h"

/**
 * IMossClockSource — 순수 C++ 인터페이스 (UObject 미상속)
 *
 * Time & Session System이 시간 값을 읽는 유일한 진입점.
 * production에서는 FRealClockSource, 테스트에서는 FMockClockSource로 교체.
 * ADR-0001 §Key Interfaces 원문 준수.
 *
 * 주의: 이 인터페이스에 시각 조작 탐지·의심 분기를 추가하는 것은
 * ADR-0001에 의해 금지된다.
 */
class IMossClockSource
{
public:
    /** 가상 소멸자 — 파생 클래스의 올바른 소멸 보장 */
    virtual ~IMossClockSource() = default;

    /** 현재 UTC 시각을 반환한다. wall clock 기반. */
    virtual FDateTime GetUtcNow() const = 0;

    /** 앱 기동 이후 경과 시간(초)을 반환한다. 단조 증가 보장(플랫폼 지원 범위 내). */
    virtual double GetMonotonicSec() const = 0;

    /**
     * 플랫폼 suspend(절전 진입) 직전에 호출된다.
     * 기본 구현은 no-op. FRealClockSource / FMockClockSource가 필요 시 재정의.
     */
    virtual void OnPlatformSuspend() {}

    /**
     * 플랫폼 resume(절전 해제) 직후에 호출된다.
     * 기본 구현은 no-op. FRealClockSource / FMockClockSource가 필요 시 재정의.
     */
    virtual void OnPlatformResume() {}
};

/**
 * FRealClockSource — production 구현체
 *
 * ADR-0001에 의해, 표준 UE 시간 API(FDateTime::UtcNow / FPlatformTime::Seconds)의
 * 직접 호출은 이 클래스 내부에만 허용된다.
 * 다른 어떤 클래스·함수에서도 해당 API를 직접 호출해서는 안 된다.
 */
class FRealClockSource : public IMossClockSource
{
public:
    /** FDateTime::UtcNow() 래핑 — production 경로 */
    virtual FDateTime GetUtcNow() const override { return FDateTime::UtcNow(); }

    /** FPlatformTime::Seconds() 래핑 — production 경로 */
    virtual double GetMonotonicSec() const override { return FPlatformTime::Seconds(); }
};

/**
 * FMockClockSource — test-only 구현체
 *
 * 테스트에서 시간을 직접 주입하여 결정론적(deterministic) 검증을 가능하게 한다.
 * Shipping 빌드에서는 완전히 제외된다 (WITH_AUTOMATION_TESTS 가드).
 *
 * 사용 예:
 *   FMockClockSource Mock;
 *   Mock.SetUtcNow(FDateTime(2026, 4, 19));
 *   TestEqual(TEXT("주입 값 일치"), Mock.GetUtcNow(), FDateTime(2026, 4, 19));
 */
#if WITH_AUTOMATION_TESTS
class FMockClockSource : public IMossClockSource
{
public:
    /** 테스트에서 반환할 UTC 시각을 주입한다 */
    void SetUtcNow(FDateTime InTime) { MockedUtc = InTime; }

    /** 테스트에서 반환할 단조 시간(초)을 주입한다 */
    void SetMonotonicSec(double InSec) { MockedMono = InSec; }

    /** OnPlatformSuspend() 호출을 시뮬레이션한다 */
    void SimulateSuspend() { OnPlatformSuspend(); }

    /** OnPlatformResume() 호출을 시뮬레이션한다 */
    void SimulateResume() { OnPlatformResume(); }

    /** 주입된 UTC 시각을 반환한다 */
    virtual FDateTime GetUtcNow() const override { return MockedUtc; }

    /** 주입된 단조 시간(초)을 반환한다 */
    virtual double GetMonotonicSec() const override { return MockedMono; }

private:
    /** 기본값: 에포크(0 ticks). 테스트에서 반드시 SetUtcNow로 초기화할 것 */
    FDateTime MockedUtc = FDateTime(0);

    /** 기본값: 0.0초. 테스트에서 반드시 SetMonotonicSec으로 초기화할 것 */
    double MockedMono = 0.0;
};
#endif // WITH_AUTOMATION_TESTS
