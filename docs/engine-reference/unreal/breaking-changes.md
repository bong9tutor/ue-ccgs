# UE Breaking Changes — Relevant to This Project

*Last verified: 2026-04-16*
*Project pinned version: Unreal Engine 5.6*

> 이 문서는 **이 프로젝트(Moss Baby)의 기술 프로필과 관련된** 변경점만 집계합니다. UE 5.6의 전체 브레이킹 체인지 목록은 [공식 Release Notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-6-release-notes)를 참조하세요.

---

## 5.5 → 5.6

### Gameplay Camera System — auto-spawn 제거 (**HIGH 영향, 그러나 이 프로젝트는 미사용**)

5.6부터 Gameplay Camera System의 자동 스폰 기능이 제거됨. Camera System Actor 자체는 유지되지만 auto-spawn 관련 로직이 사라짐.

- 부작용: `OwnerSee` / `OwnerNoSee` 프로퍼티 일부 동작 불가, 특정 level streaming 케이스 영향
- 일부 Blueprint Camera Directors / Camera Node Evaluators가 `UE_DEPRECATED`로 표기되고 대체 API 안내 메시지 포함
- **이 프로젝트와의 연관성**: Moss Baby는 **고정 단일 카메라**(유리병 클로즈업)를 사용하므로 Gameplay Camera System 미사용. 다만 향후 시네마틱(작별 씬)에서 Sequencer를 사용할 경우 **새 API를 쓸 것** — 구 Gameplay Camera 경로 금지.
- 참고: [UE5 Gameplay Cameras — Upgrading to 5.6 (Ludovic Chabant)](https://ludovic.chabant.com/blog/2025/06/06/ue5-gameplay-cameras-upgrading-to-5-6/)

### Animation Asset 자동 변환 이슈 (**MEDIUM 영향, 이 프로젝트는 영향 없음**)

5.5에서 저작한 애니메이션을 5.6에서 열면 **rotation keyframe 값이 편집 없이도 자동 변환**되어 다른 포즈가 나올 수 있음.

- 완화: 축별 값을 5.5에서 수동 복사하면 해결
- **이 프로젝트와의 연관성**: 캐릭터 애니메이션 없음(이끼 아이는 머티리얼 파라미터·블렌드 셰이프 구동) → **영향 없음**

### Rendering Pipeline / PSO 컴파일 방식 변경 (**MEDIUM 영향, 주의**)

5.6에서 셰이더/PSO(Pipeline State Object) 처리 방식이 바뀌어 **런타임 중 PSO를 점진적으로 컴파일**함. 최초 로드 시 모든 permutation을 준비하던 이전 동작과 다름.

- 부작용: 머티리얼·셰이더가 많은 에셋이 **점진적으로 렌더링**되면서 히칭/팝인이 보일 수 있음
- **이 프로젝트와의 연관성**: Moss Baby는 에셋 수가 매우 적음(유리병·이끼·카드·UI) → **실질적 영향은 미미**하나, **PSO 캐시 쿡 체크**(PSO Precaching)를 릴리스 빌드에서 검증할 것.
- 완화 전략: Project Settings → Rendering → "PSO Precaching" 활성화, 릴리스 빌드에서 첫 실행 시 프리컴파일 단계 고려.

### Open World / Streaming 관련 (**영향 없음**)

- Fast Geometry Streaming Plugin (Experimental) — 대규모 오픈월드 대상. 이 프로젝트 무관.
- World Partition 개선 — 이 프로젝트는 단일 씬이므로 무관.

---

## 5.4 → 5.5

### Animation Authoring 대규모 개편 (**MEDIUM 영향, 그러나 제한적**)

엔진 중심 애니메이션·리깅 툴셋 도입. 외부 DCC 왕복 감소.

- **이 프로젝트와의 연관성**: 캐릭터 애니메이션 없으므로 대부분 무관. 다만 시네마틱 씬에서 Control Rig를 쓸 경우 5.5+ 패턴을 사용할 것.

### Mobile Game Development 개선 (**영향 없음**)

- 모바일 렌더러·쉐이더 성능 향상. 이 프로젝트는 데스크톱 전용이므로 무관.

### In-camera VFX / Virtual Production production-ready (**영향 없음**)

- 이 프로젝트와 무관.

---

## Migration Strategy (프로젝트에 적용 가능한 경로)

현재 `MadeByClaudeCode/` 은 **UE 5.6 Blank C++ 프로젝트로 새로 생성**되었으므로 마이그레이션이 필요하지 않음. 버전 업그레이드는 이후 `/setup-engine upgrade 5.6 [target]` 으로 처리.

## Sources

- [UE 5.6 Release Announcement](https://www.unrealengine.com/news/unreal-engine-5-6-is-now-available)
- [UE 5.6 Release Notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-6-release-notes)
- [UE 5.5 Release Notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-5-release-notes)
- [UE 5.6 Performance Highlights — Tom Looman](https://tomlooman.com/unreal-engine-5-6-performance-highlights/)
- [UE5 Gameplay Cameras: Upgrading to 5.6 — Ludovic Chabant](https://ludovic.chabant.com/blog/2025/06/06/ue5-gameplay-cameras-upgrading-to-5-6/)
- [Project migrate from 5.5 to 5.6 — Forum](https://forums.unrealengine.com/t/project-migrate-from-5-5-to-5-6/2602479)
