"""Verify a direct gift and its recovery by the queen in the official engine."""
from pathlib import Path
import re
import sys
from unswbc.engine import EngineModule
from unswbc.bot import Bot, Pool

w = h = 20
lines = [f'MAP {w} {h}', 'MAP_NAME Direct Gift', f'TILE_COUNT {w*h}']
lines += [f'TILE {x} {y} 0 0' for y in range(h) for x in range(w)]
edges = [f'EDGE {r*(w+1)+x} {int(r in (20,22) and x==11)} -1'
         for r in range(2*h+1) for x in range(w+r%2)]
lines += [f'EDGE_COUNT {len(edges)}', *edges, 'DRAGON_COUNT 3',
          'DRAGON 0 8 10 10 9 10 8 10 7 10 6 10 6 11 7 11 8 11',
          'DRAGON 1 2 2 2 1 2', 'DRAGON 0 2 13 9 14 9', 'END']
small = len(sys.argv)>2 and sys.argv[2]=='small'
if small:
    lines=[('DRAGON 0 2 10 10 9 10' if line.startswith('DRAGON 0 8 ') else
            'DRAGON 0 4 13 10 14 10 14 9 13 9' if line=='DRAGON 0 2 13 9 14 9' else line) for line in lines]
if small:
    lines=[re.sub(r'^(EDGE \d+) \d+ -1$',r'\1 0 -1',line) for line in lines]
engine = EngineModule()
pool = Pool([str(Path(sys.argv[1]).resolve())], size=1)
live, teams, lengths, indicators = {}, {}, [], []

class Enough(Exception):
    pass

def spawn(i, init):
    teams[i] = next(s.split()[1] for s in init.decode().splitlines() if s.startswith('TEAM '))
    if teams[i] == 'A':
        live[i] = Bot(pool, init=init, name=str(i))

def reply(i, block):
    rnd = int(block.split()[1])
    if rnd >= 32:
        raise Enough()
    if teams[i] == 'B':
        return b'MOVE E\nENDTURN\n'
    if i == 0:
        lengths.append(int(re.search(rb'LENGTH (\d+)', block).group(1)))
    shifted = re.sub(rb'^ROUND \d+', f'ROUND {rnd+(300 if small else 410)}'.encode(), block)
    out = live[i].ask(shifted)
    assert not live[i].error, live[i].error
    indicators.extend(line for line in out.decode().splitlines() if line.startswith('INDICATOR '))
    return out

def death(i, _round, _reason):
    bot = live.pop(i, None)
    if bot:
        bot.stop()

try:
    try:
        engine.run(('\n'.join(lines)+'\n').encode(), reply, death, spawn, lambda _: None, seed=101)
    except Enough:
        pass
finally:
    for bot in live.values():
        bot.stop()
    pool.close()
assert any('DONATE_DIRECT' in line or 'DONATE_NEAR' in line or 'DONATE_ACCEPTED' in line for line in indicators), indicators
assert max(lengths, default=0) >= (4 if small else 9), lengths
print('PASS: sonar length authorized a direct gift; queen recovered it:', lengths)
