"""Schooltime-style initially coiled queen keeps its fixed ID via a newborn tail gift."""
from pathlib import Path
import re,sys
from unswbc.engine import EngineModule
from unswbc.bot import Bot,Pool

w=h=40
body=[(10,10),(10,9),(11,9),(11,10)]
walls={(2*10+2)*(w+1)+10,(2*10+1)*(w+1)+10}
lines=[f'MAP {w} {h}','MAP_NAME Queen Tail Gift',f'TILE_COUNT {w*h}']
lines += [f'TILE {x} {y} 0 0' for y in range(h) for x in range(w)]
edges=[f'EDGE {r*(w+1)+x} {int(r*(w+1)+x in walls)} -1' for r in range(2*h+1) for x in range(w+r%2)]
lines += [f'EDGE_COUNT {len(edges)}',*edges,'DRAGON_COUNT 2',
          'DRAGON 0 4 '+' '.join(f'{x} {y}' for x,y in body),
          'DRAGON 1 2 22 30 21 30','END']
pool=Pool([str(Path(sys.argv[1]).resolve())],size=1)
live={};teams={};deaths=[];actions=[];observed={}
class Enough(Exception):pass

def spawn(i,init):
    teams[i]=next(l.split()[1] for l in init.decode().splitlines() if l.startswith('TEAM '))
    if teams[i]=='A':live[i]=Bot(pool,init=init,name=str(i))
def reply(i,block):
    rnd=int(block.split()[1])
    if rnd>=10:raise Enough()
    if teams[i]=='B':return b'MOVE E\nENDTURN\n'
    observed[i]=int(re.search(rb'LENGTH (\d+)',block).group(1))
    out=live[i].ask(block);assert not live[i].error,live[i].error
    actions.append((i,rnd,out.decode()));return out
def death(i,r,reason):
    deaths.append((i,r,reason));bot=live.pop(i,None)
    if bot:bot.stop()
try:
    try:EngineModule().run(('\n'.join(lines)+'\n').encode(),reply,death,spawn,lambda _:None,seed=101)
    except Enough:pass
    assert 0 in live,'The scoring queen must survive'
    assert any(i==0 and r==0 and 'SPLIT 2\n' in out for i,r,out in actions),actions
    assert any(i==2 and r==0 and 'QUEEN_TAIL_GIFT_2' in out for i,r,out in actions),actions
    assert deaths==[(2,0,'A')],deaths
    assert observed[0]==3,observed
    assert any(i==0 and r==1 and 'MOVE E\n' in out for i,r,out in actions),actions
    print('PASS: initially blocked queen split2, newborn2 died, fixed queen ate its pearl and survived at length3 for 10 rounds')
finally:
    for bot in live.values():bot.stop()
    pool.close()
