# UE 5.6 Deprecated APIs — Project-Relevant

*Last verified: 2026-04-16*
*Project pinned version: Unreal Engine 5.6*

> 이 표는 **이 프로젝트(Moss Baby)에서 사용할 가능성이 있는 영역**의 deprecated 항목만 나열합니다. 전체 목록은 [UE_DEPRECATED 매크로 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/UE_DEPRECATED) 와 공식 릴리스 노트를 참조.

---

## 에이전트 사용 가이드

- C++ API를 제안하기 전에 이 표를 확인
- 이 표에 없는 API는 UE 5.6에서 **일반적으로 유효**(단, WebSearch로 재확인 가능)
- 제안하는 API에 `UE_DEPRECATED(5.x, ...)` 매크로가 보이면 대체 API 사용

---

## Deprecated in 5.6

### Gameplay Camera System — Auto-Spawn 경로

| Don't Use | Use Instead | Notes |
|---|---|---|
| Camera System Actor 자동 스폰 로직 | 명시적 카메라 Component 배치 + 필요 시 Sequencer | 5.6에서 auto-spawn 제거. [참조](https://ludovic.chabant.com/blog/2025/06/06/ue5-gameplay-cameras-upgrading-to-5-6/) |
| Blueprint Camera Directors (일부) | Deprecated 메시지의 대체 노드 안내 참조 | 노드 자체가 deprecated 태그를 표시 |
| Blueprint Camera Node Evaluators (일부) | 상동 | 상동 |
| `OwnerSee` / `OwnerNoSee` 프로퍼티 (일부 조합) | Component 기반 visibility 제어 | 새 Camera Component와 충돌 가능 |

**이 프로젝트 영향**: Moss Baby는 **단일 고정 카메라 + 작별 씬 시퀀서** 사용 예정. Gameplay Camera System 미사용이므로 직접적 블로커 없음. 다만 **새 코드에서 Gameplay Camera System의 auto-spawn 경로 사용 금지**.

---

## Deprecated in 5.5 (여전히 5.6에서도 deprecated)

이 프로젝트에서 영향 큰 항목 없음 (캐릭터 애니메이션·모바일·대규모 오픈월드 미사용).

---

## 주의할 Pattern — 일반 UE C++

이 프로젝트가 자주 쓸 영역에서 **deprecated되지는 않았지만 피해야 할** 오래된 패턴:

| Pattern | Why | Prefer |
|---|---|---|
| Raw `UPROPERTY` 없는 UObject 필드 | GC 안전성 | 항상 `UPROPERTY()` 표기 |
| `TSubobjectPtr` (UE4 잔재) | Legacy | `TObjectPtr<>` (UE 5.0+) |
| `CreateDefaultSubobject<T>` 반환을 raw `T*`로 저장 | TObjectPtr 권장 | `TObjectPtr<T>` 필드 |
| Blueprint에서 `Delay` 노드로 장기간 대기 | 세이브 로드 시 날아감 | Timer Handle + 명시적 세이브 통합 |
| `FTimerManager` 로 21일 실시간 추적 | 앱이 닫히면 손실 | `FDateTime::UtcNow()` 기반 경과 계산 + 세이브 |

---

## 작별 씬 구현 시 주의

시네마틱(Day 21) 구현 시 Sequencer 기반. 다음 API 우선:

- `UMovieSceneSequencePlayer` (런타임 플레이어)
- `ULevelSequenceActor` (배치 기반)
- Gameplay Camera의 auto-spawn 경로 **금지** (위 deprecated 섹션 참조)

---

## Sources

- [UE_DEPRECATED Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/UE_DEPRECATED)
- [UE 5.6 Release Notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-6-release-notes)
- [UE5 Gameplay Cameras: Upgrading to 5.6](https://ludovic.chabant.com/blog/2025/06/06/ue5-gameplay-cameras-upgrading-to-5-6/)
