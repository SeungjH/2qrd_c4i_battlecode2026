"""Reconstruct the M844581 round-30 tail ambush with the official engine."""
from pathlib import Path
import sys
from unswbc.engine import EngineModule
from unswbc.bot import Bot, Pool

w, h = 32, 16
lines = [f'MAP {w} {h}', 'MAP_NAME Tail Ambush', f'TILE_COUNT {w*h}']
lines += [f'TILE {x} {y} {int((x,y)==(16,8))} {int((x,y)==(16,8))}'
          for y in range(h) for x in range(w)]
edges = [f'EDGE {r*(w+1)+x} 0 -1' for r in range(2*h+1) for x in range(w+r%2)]
lines += [f'EDGE_COUNT {len(edges)}', *edges, 'DRAGON_COUNT 4',
          'DRAGON 0 2 2 12 1 12', 'DRAGON 1 2 25 12 24 12',
          'DRAGON 0 4 15 5 15 6 15 7 15 8',
          'DRAGON 1 4 16 7 16 6 16 5 16 4', 'END']
engine = EngineModule()
pool = Pool([str(Path(sys.argv[1]).resolve())], size=1)
live, teams, actions, deaths = {}, {}, [], []

class Enough(Exception):
    pass

def spawn(i, init):
    teams[i] = next(s.split()[1] for s in init.decode().splitlines() if s.startswith('TEAM '))
    if teams[i] == 'A':
        live[i] = Bot(pool, init=init, name=str(i))

def reply(i, block):
    if int(block.split()[1]) >= 1:
        raise Enough()
    if teams[i] == 'B':
        return b'MOVE S\nENDTURN\n'
    out = live[i].ask(block)
    assert not live[i].error, live[i].error
    actions.extend((i, line) for line in out.decode().splitlines() if line.startswith(('MOVE ', 'SPLIT ', 'INDICATOR ')))
    return out

def death(i, rnd, reason):
    deaths.append((i, rnd, reason))
    bot = live.pop(i, None)
    if bot:
        bot.stop()

try:
    try:
        engine.run(('\n'.join(lines)+'\n').encode(), reply, death, spawn, lambda _: None, seed=101)
    except Enough:
        pass
    assert 2 in live, 'Parent must survive'
finally:
    for bot in live.values():
        bot.stop()
    pool.close()
assert (2, 'SPLIT 2') in actions, actions
assert (4, 'MOVE E') in actions, actions
assert any('TAIL_AMBUSH_SPLIT' in line for _, line in actions), actions
assert any('TAIL_AMBUSH_TRADE' in line for _, line in actions), actions
assert {i for i, _, reason in deaths if reason=='H'} == {3, 4}, deaths
print('PASS: parent kept 2; newborn 2 traded for enemy 5 in the split round')
