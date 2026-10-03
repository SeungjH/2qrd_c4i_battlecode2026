# v15 검증 결과

## 비교 조건

- 기준: GitHub v14 `70fe448d9a0cb2f465ef9a3bff1c6ab7a176638d`의 정확한 소스. `tests/baselines/v14`에 보존했습니다.
- 공식 `unswbc 1.2.3` 엔진. 실제 맵 6개 × 시드 101/202/303/404/505/606/707/808 × 진영 교대 = **96개 경기 조건**.
- 최종 소스를 고정한 뒤 맥 네이티브 96경기 + 공식 심판 WASM 96경기 + Windows 네이티브 12경기, **총 204경기** 실행했습니다. 반복 검증은 독립 표본을 늘리지 않으므로 주 성적의 표본은 96입니다.
- 설치된 toolkit의 WASM 캐시는 .cpp만 해시하고 헤더 변경을 놓칩니다. `tests/build_wasm.py`는 제출 헤더 내용까지 해시한 별도 스테이징을 사용합니다. 오래된 캐시로 실행한 개발 대전은 성적에서 제외했습니다. 제출 파일로 새로 빌드한 WASM과 최종 대전 WASM의 SHA-256이 같았습니다.
- 동일한 한 Autarky 경기 전체의 네이티브/WASM 출력도 바이트 단위로 대조했습니다. 전체 비교에서는 일부 컴파일 환경 차이가 있으므로 공식 심판 결과를 주 성적으로 삼습니다.

## 공식 심판 성적

**v15 53승 / v14 43승 / 무승부 0 — v15 승률 55.2%.**

| 맵 | v15 승 | v14 승 |
|---|---:|---:|
| autarky | 8 | 8 |
| default | 8 | 8 |
| dilemma | 9 | 7 |
| portals | 10 | 6 |
| queen_of_spades | 8 | 8 |
| schooltime | 10 | 6 |

초기 네이티브 72조건에서는 43승 29패였지만 추가 24조건에서는 10승 14패였습니다. 전체 네이티브 96조건과 최종 WASM 96조건의 승수는 모두 53입니다. 그렇다고 확실한 범용 승률 향상이라고 단정할 결과는 아닙니다. 단순 Wilson 95% 구간은 약 45.3~64.8%이며, 맵/시드와 진영 교대 경기의 상관도 고려해야 합니다. 이는 v14와의 직접 대전 승률이고 대회 전체 상대에 대한 승률 증가분이 아닙니다.

Windows 확인 대전은 8승 4패였습니다. 컴파일/실행 환경별로 세부 결과가 일부 달랐습니다.

## 개선 지표

| 지표 (공식 96조건) | v14 | v15 |
|---|---:|---:|
| Queen 경기 종료 생존 | 8 | 20 |
| Queen 평균 생존 라운드 (생존 종료=500) | 193.8 | 228.9 |
| 의도적 증여 행동 | 256 | 395 |
| Queen 긴급 뒤쪽 분열 | 2 | 21 |
| Queen 출구를 비우는 행동 | 0 | 116 |

행동 횟수는 indicator에서 실제 선택을 집계합니다. 전체 경기의 증여 진주가 모두 수혜자에게 들어갔다고 가정하지 않습니다. 회수와 독점 섭취는 아래 별도 시나리오에서 검증했습니다.

## 회귀와 실제 엔진 시나리오

- `tests/run.sh`: **58개 전략 fixture + 프로세스 행동 회귀 통과**. 200턴 스폰 주변 Queen 생존, 일반 탐색/분열/공격 및 기존 제한 스프린트 검사가 포함됩니다.
- Queen 꼬리 막힘: 뒤쪽 2칸 분리, 새 자식만 제자리 사망, 원래 ID0가 진주를 먹고 10라운드 생존.
- Queen 중간 몸통 막힘: 길이 11에서 최소 뒤쪽 4칸 분리, 자식만 사망, 원래 ID0가 길이 9로 살아남음. v14는 동일 시나리오에서 Queen이 죽습니다.
- 다중 증여 3개 방향: Queen 최종 길이 **17/17/14**, 확인된 선물 섭취 9/9/6, 다른 아군의 선물 섭취 **0**. v14도 이 시험을 통과하며, 모든 방향의 수집량이 v15에서 더 늘어난 것은 아닙니다.
- 확정 Queen 사망 후 대체 수혜자: 증여 3/2/2회, 최종 길이 **16/12/17**, 확인된 수혜자의 선물 섭취 각 2회, 다른 아군 섭취 **0**. v14는 첫 방향부터 증여 0회로 실패합니다. 실패를 주입한 Queen을 제외한 아군은 생산 봇으로 자율 행동합니다.
- 보지 못한 Queen 사망: 3개 방향 모두 잘못된 희생 허가 없음.
- 일반 개체의 역방향 머리 2칸 희생, 기존 꼬리 공격, 길이 2/4 Queen 증여 회수, 공식 Queen 점수 우선 규칙, 기존 제한 스프린트 엔진 검사 통과.
- UBSan 9개 중요 fixture 통과. ASan은 빈 main()조차 런타임 시작에서 멈춰 통과로 집계하지 않았습니다.
- **기존 실패 1건:** `engine_trap.py`의 경고 전달 assertion은 v14와 v15 모두 실패합니다. 이번 Queen/증여 수정 범위에 포함하지 않았으며 해결했다고 주장하지 않습니다.

## 공식 심판 제한

- 총 2,084,445개 드래곤 턴, 런타임 오류/시간 초과 **0건**.
- 최대 턴 비용 8,689,995 points, 100,000,000 제한 이내.
- 최대 관측 선형 메모리 589,824 bytes, 48MiB 제한 이내.
- 네이티브와 공식 심판은 96조건 중 90조건의 최종 결과가 같았습니다.

## 재현

```sh
./tests/run.sh
clang++ -std=c++20 -O2 main.cpp -o /tmp/v15
python tests/engine_queen_tail_gift.py /tmp/v15
python tests/engine_queen_tail_gift.py /tmp/v15 middle
python tests/engine_feeding.py /tmp/v15
python tests/engine_feeding.py /tmp/v15 --certified-fallback
python tests/engine_feeding.py /tmp/v15 --fallback

python tests/build_wasm.py . /tmp/v15.wasm
python tests/build_wasm.py tests/baselines/v14 /tmp/v14.wasm
python tests/compare_versions.py --candidate /tmp/v15.wasm --baseline /tmp/v14.wasm \
  --maps tests/maps/autarky.map tests/maps/default.map tests/maps/portals.map \
         tests/maps/schooltime.map tests/maps/queen_of_spades.map tests/maps/dilemma.map \
  --seeds 101 202 303 404 505 606 707 808 --workers 4 --output /tmp/v15-results.json
```

검증에 사용한 Python 환경은 `unswbc==1.2.3`입니다. tests/기준 봇/맵/결과 파일은 제출 ZIP에서 제외합니다.

원시 결과: [공식 96경기](v15_wasm_96.json), [네이티브 초기 72](v15_native_72.json), [추가 24](v15_native_holdout_24.json), [Windows 12](v15_windows_12.json), [시나리오](v15_scenario_results.json), [제출 소스 해시](v15_source_hashes.json).
