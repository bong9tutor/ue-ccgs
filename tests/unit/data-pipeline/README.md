# Data Pipeline Unit Tests

Story: foundation-data-pipeline / Story 1-4 (Schema Types)
Epic: foundation-data-pipeline
Layer: Foundation

## 실제 테스트 파일

```
MadeByClaudeCode/Source/MadeByClaudeCode/Tests/DataPipelineSchemaTests.cpp
```

## 테스트 카테고리

```
MossBaby.Data.Schema.*
```

## 헤드리스 실행 명령

```bash
UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
  -ExecCmds="Automation RunTests MossBaby.Data.Schema.; Quit" \
  -nullrhi -nosound -log -unattended
```

## AC 매핑

| Automation Test | AC | 타입 | Gate |
|---|---|---|---|
| `MossBaby.Data.Schema.DataPipelineSettingsCDO` | AC-5 (UMossDataPipelineSettings 9 knobs CDO) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Schema.FinalFormAssetGetPrimaryAssetId` | AC-2 (UMossFinalFormAsset GetPrimaryAssetId) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Schema.DreamDataAssetGetPrimaryAssetId` | AC-3 (UDreamDataAsset GetPrimaryAssetId) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Schema.DreamDataAssetDefaults` | AC-3 (DreamDataAsset C2 schema gate 기본값) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Schema.StillnessBudgetAssetDefaults` | AC-4 (UStillnessBudgetAsset 기본값) | AUTOMATED | BLOCKING |
| C1 schema gate CI grep (아래 참조) | AC-6 (FGiftCardRow 수치 필드 금지) | CODE_REVIEW | BLOCKING |

## C1 Schema Gate grep (AC-6 — CODE_REVIEW)

FGiftCardRow에 수치 효과 필드(int32/float/double)가 없는지 CI에서 검증:

```bash
grep -rE "(int32|float|double) [A-Z][A-Za-z]*" \
  MadeByClaudeCode/Source/MadeByClaudeCode/Data/GiftCardRow.h
```

기대 결과: **0 매치** (매치 발생 시 C1 schema gate 위반 — PR 차단)

허용 타입: `FName`, `TArray<FName>`, `FText`, `FSoftObjectPath`만.

## 관련 ADR

- ADR-0002: Data Pipeline 컨테이너 선택 (C1/C2 schema gate 정의)
- ADR-0010: FFinalFormRow 저장 형식 — UPrimaryDataAsset 격상
- ADR-0011: Tuning Knob 노출 방식 — UDeveloperSettings 채택
