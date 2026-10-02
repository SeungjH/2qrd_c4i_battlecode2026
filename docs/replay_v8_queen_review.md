# v8 리플레이와 Sol5.6 분석 대조

두 파일 모두 `botA=14548` 및 `INDICATOR v8`로 우리 봇은 raw Team A임을 확인했습니다. 사용자가 알려준 화면 색은 주황입니다.

| 경기 | 우리 고정 queen | 상대 고정 queen | 종료 |
|---|---|---|---|
| M851170 / Autarky | ID 0, R25 길이 3에서 body 사망, PORTAL_SCOUT | ID 1, R354 길이 6에서 body 사망 | 승리; 최장 23 대 18 |
| M851171 / Trauma | ID 1, R84 길이 3에서 self 사망, NO_LEGAL_ESCAPE | ID 0, 끝까지 생존, 길이 4 | 패배; queen 0 대 4 |

공식 현재 점수 계산은 queen → 최장 → 총 길이 순입니다. 두 경기의 순위와 고정 queen 사망 시각은 Sol 분석과 일치합니다. v8 소스에서 `electQueen()`이 가장 긴 아군을 선택하는 것도 직접 확인했습니다. 이름이 queen이었지만 공식 queen을 보존·지원한 로직이 아니었습니다.

## 독립적으로 재현한 수치

- Autarky: self 110 / head 14 / body 10 / wall 2. NO_LEGAL_ESCAPE self 103회. 아군 사체 진주 202개 중 아군 176개, 적 16개 회수.
- Trauma: self 147 / head 4 / wall 4. NO_LEGAL_ESCAPE self 145회. 아군 사체 진주 203개 중 아군 172개, 적 15개 회수.
- Trauma의 NO_LEGAL_ESCAPE self 사망 중 자식 출신: LAST_RESORT_SPLIT_2 **62**, POCKET_ESCAPE_3 **34**, SCOUT_SPLIT_2 **30**, REVERSE_RESCUE **15**. Sol의 수치와 일치합니다.
- 두 경기의 instructions.exceeded는 모두 0입니다. invalid 사망도 없습니다.
- 벽 사망은 Autarky R456 ID209·R458 ID204, Trauma R410 ID182·R459 ID214·R467 ID280·R482 ID294이며 모두 DONATE 지시와 대응합니다.
- Autarky R422에 상대 최장 ID191(길이 25)이 포털 0으로 이동한 뒤 body 사망했습니다. 당시 상대 길이는 25·12·4·2였습니다. 사용자가 지적한 상대의 실수를 재현했습니다.

## 해석의 범위

Sol의 핵심 수치와 코드 결함 지적은 독립 검증되어 신뢰할 수 있습니다. 다만 NO_LEGAL_ESCAPE라는 표시만으로 이전 턴에 탈출 경로가 있었다거나 모든 비상 분열이 손해라고 결론 내릴 수는 없습니다. 사체 회수율 약 87%·85%도 그 진주가 공식 queen에게 돌아갔음을 의미하지 않습니다.

Trauma queen은 R79 길이 5에서 R80 길이 2, R83 길이 5에서 R84 길이 3으로 줄었습니다. 원래 ID를 작은 부모로 남기는 분열 뒤에도 공식 queen은 그 부모입니다. 긴 자식이 살아남았다는 이유로 queen 보호에 성공했다고 볼 수 없습니다.

따라서 사망 수를 무조건 줄이는 대신 **고정 queen 생존, 증여 진주의 queen 회수, 분열 후 부모·자식의 실질 수익**을 분리해 평가해야 합니다.
