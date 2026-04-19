# Unreal Engine — Version Reference

*Last verified: 2026-04-16*

| Field | Value |
|-------|-------|
| **Engine Version** | Unreal Engine 5.6 (hotfix 5.6.1 권장) |
| **Project Pinned** | 2026-04-16 |
| **LLM Knowledge Cutoff** | May 2025 |
| **Risk Level** | **HIGH** — UE 5.6은 2025년 6월 출시로 학습 데이터 이후. API 제안 전 이 문서와 `deprecated-apis.md` 반드시 확인. |

## Post-Cutoff Version Timeline

| Version | Release | Relevance to This Project |
|---------|---------|---------------------------|
| 5.4 | 2024년 초 | LLM 학습 커버리지 초기 경계 |
| 5.5 | 2024년 말 | 애니메이션 저작·모바일·VFX production-ready — **컷오프 근접** |
| **5.6** | **2025년 6월** | **이 프로젝트 핀 버전**. Open world 60 FPS 초점, HWRT Lumen 개선, 애니메이션 엔진 중심 워크플로우, MetaHumans 인엔진 저작. 학습 데이터 밖. |
| 5.6.1 | 2025년 여름 | 270+ 수정 핫픽스. MetaHumans / PCG / Motion Design 안정화. |
| 5.7 | 2025년 하반기 ~ 2026 | 이 프로젝트는 **5.6에 핀**. 5.7 변경은 이후 `/setup-engine upgrade`로 검토. |

## Project-Relevant 5.6 Highlights

Moss Baby의 기술 프로필(작은 데스크톱 씬, 정적 메시 중심, Lumen 조명, UMG UI, 로컬 세이브)에 **직접 영향**이 있는 5.6 변경점:

- **Lumen HWRT CPU bottleneck 개선** — 데스크톱 황혼 씬의 GI 품질·성능 확보에 유리
- **Animation engine-first workflow** — 이 프로젝트는 캐릭터 애니메이션 미사용(이끼 아이는 머티리얼·블렌드 셰이프 구동)이므로 영향 **낮음**
- **Fast Geometry Streaming Plugin (Experimental)** — Open world용, 이 프로젝트와 **무관**
- **MetaHumans in-engine authoring** — **무관**
- **Gameplay Camera System 변경** — auto-spawn 제거됨. 이 프로젝트는 고정 카메라이므로 주의하되 큰 영향 없음 → `deprecated-apis.md` 참조

## Related Reference Documents

- [`breaking-changes.md`](./breaking-changes.md) — 버전별 브레이킹 체인지
- [`deprecated-apis.md`](./deprecated-apis.md) — Don't use / Use this 대응 표
- [`current-best-practices.md`](./current-best-practices.md) — Moss Baby 관점의 UE 5.6 권장 사용법

## Official References

- [UE 5.6 Release Announcement (Epic)](https://www.unrealengine.com/news/unreal-engine-5-6-is-now-available)
- [UE 5.6 Release Notes (dev.epicgames.com)](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-6-release-notes)
- [UE 5.6 Released — Forum Announcement](https://forums.unrealengine.com/t/unreal-engine-5-6-released/2538952)
- [UE 5.6 Performance Highlights — Tom Looman](https://tomlooman.com/unreal-engine-5-6-performance-highlights/)
- [UE 5.6.1 Hotfix — Forum](https://forums.unrealengine.com/t/5-6-1-hotfix-released/2639316)

## Maintenance

- 에이전트/스킬이 UE API를 제안할 때 이 문서를 먼저 읽을 것 (Version Awareness)
- 엔진 업그레이드 시 `/setup-engine upgrade 5.6 [new]` 실행
- 레퍼런스 최신화는 `/setup-engine refresh`
