# 봇 소스 구조

## 진입점과 빌드

- `entry.cpp`: 공식 제출의 유일한 번역 단위. 정적 저장소의 `Bot` 한 개를 실행한다. 큰 월드 배열을 WASM 스택에 올리지 않는다.
- 루트 `main.cpp`: 기존 `clang++ main.cpp` 명령을 유지하는 호환 래퍼. 공식 제출에는 포함하지 않는다.
- `controller.hpp`: 전체 구현을 모으는 umbrella header.
- `bot.hpp`: `Bot`의 public `run()`과 private 기능 선언. 상태는 private `BotState`로 소유하며 복사를 금지한다.
- `types.hpp`: 좌표·관측·후보 이동·패킷·턴 계획과 기존 정책 상수. 프로젝트 namespace 안에만 둔다.
- `state.hpp`: 관측/월드/탐색/통신/먹이지원/전투의6개 데이터 그룹. 전역 가변 상태가 아니며 Bot마다 별도 메모리를 갖는다. 데이터 전용 private 상속은 기존 필드 이름을 보존하는 이행 수단이고, 가상 전략 계층이 아니다.

공식 `bot.toml`은 `src/entry.cpp`와 `src/*.hpp`만 선택한다. 공식 도구가 파일의 basename도 매칭하기 때문에 루트 `main.cpp`를 지정하면 분석 폴더·가상환경의 동명 파일까지 수집될 수 있다. 실제 선택된15파일/단일.cpp 및 공식 clang 빌드를 검증했다. `main.cpp`와 `src/entry.cpp`를 동시에 컴파일하면 안 된다. 구현은 .hpp로 유지한다.

## 역할별 구현

| 파일 | 책임 |
|---|---|
| `geometry.hpp` | 좌표·거리·관측된 개체 조회 |
| `world.hpp` | 지형·포털·몸통 기억 및 이동 기록 |
| `communication.hpp` | 패킷 해석·경고·동료/먹이 보고·송신 우선순위 |
| `movement.hpp` | 충돌·공간·위험·장기 이동 안전·후보 평가 |
| `navigation.hpp` | 반복 탈출·진주/자원 경로·협소 공간·탈출 안내 |
| `feeding.hpp` | 퀸 집결·증여·수집 계획·지원 스프린트 |
| `combat.hpp` | 사냥·유리한 머리 교환·꼬리 매복 |
| `survival.hpp` | 분열·막힘·퀸 꼬리 희생·폐기 취소·긴급 포털 |
| `protocol.hpp` | 엔진 입력 및 관측 갱신 순서 |
| `turn.hpp` | 판단 우선순위·행동 실행·턴 종료 |

헤더 의존성은 `controller → 구현 헤더 → bot → state → types` 방향이다. 구현 모듈끼리 include하지 않는다. 내부 함수 호출은 Bot의 선언을 통한다. 각 모듈이 완전히 독립된 전략 객체라는 뜻은 아니며, 공유 내부 상태 의존은 아직 있다.

## 동작 보존 계약

`run → initialize → readTurn → playTurn`

`playTurn`은 다음 순서를 지킨다:
1. 폐기/새 꼬리 자식 희생 같은 종료 행동 처리.
2. 현재 턴 기억 갱신.
3. 안전 이동 후보와 경로 안내, 퀸 집결, 증여, 교환, 사냥, 꼬리 해제.
4. 아직 선택하지 못했다면 기존 우선순위대로 분열/포털/매복/탐색/최소 피해 fallback.
5. 행동 출력과 몸통 기억 갱신.
6. 소나·indicator·ENDTURN 출력 후 살아 있는 턴 수 증가.

`ActionPlan`은 그 턴의 후보·선택·분열 크기·사유만 보유하고, 장기 기억과 섞지 않는다. 종료 행동은 기존처럼 방문 기억과 턴 수를 증가시키지 않는다. 이번 리팩터링에서 가중치·임계값·전략 우선순위를 바꾸지 않았다.

## 테스트와 검증

`tests/strategy_tests.cpp`의 좁은 friend fixture만 내부 상태에 접근한다. 생산용 public 상태나 전역 호환 alias는 만들지 않았다. 상태 격리 테스트를 포함한53개 전략 fixture와 프로세스 행동 회귀를 통과했다.

`analysis/asides-v13-review/refactor/`에 리팩터링 전 소스,100경기217840턴의 전체 출력 동일성, 공식 심판3개 전체 경기5186턴의 출력 동일성·자원 검증, 원시 기록 및 재현 스크립트가 있다. 정확한 수치는 그 폴더의 `report.ko.md`를 참조한다.

## v15 전략 변경 범위

위 동작 보존 기록은 v14 리팩터링 당시의 계약이다. v15는 Queen 안전·길 비우기·최소 뒤쪽 분열·사망 증명 전파·증여 회수만 수정했다. 일반 개체의 탐색·전투·분열·스프린트 구현은 유지한다. `ActionPlan::sacrifice`는 아군 Queen의 검증된 출구를 여는 제자리 사망이며 MOVE를 출력하지 않는다. Queen의 분열은 고정 ID를 남기는 `QUEEN_BODY_RELEASE`로 처리하고 새 자식 요청에 reserved ESCAPE entry 4093을 사용한다. QUEEN payload bit25는 수혜자/진주 예약 위치, bit26은 집결 위치로 서로 구분한다.
