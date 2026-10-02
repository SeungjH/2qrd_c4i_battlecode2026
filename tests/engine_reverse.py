from pathlib import Path
from unswbc.engine import EngineModule
from unswbc.bot import Pool,Bot
import re,sys
W=H=20
body=[(10,10),(10,11),(11,11),(12,11),(12,10),(12,9),(11,9),(10,9),(9,9),(9,10)]
lines=[f'MAP {W} {H}','MAP_NAME Reverse Test',f'TILE_COUNT {W*H}']
for y in range(H):
 for x in range(W):lines.append(f'TILE {x} {y} 0 0')
edges=[]
for r in range(2*H+1):
 for x in range(W+(r%2)):
  blocked=(r%2==1 and x==11 and (r//2)%H==10)
  edges.append(f'EDGE {r*(W+1)+x} {int(blocked)} -1')
lines += [f'EDGE_COUNT {len(edges)}',*edges,'DRAGON_COUNT 2',
          'DRAGON 0 10 '+' '.join(f'{x} {y}' for x,y in body),
          'DRAGON 1 2 2 2 2 3','END']
engine=EngineModule();pool=Pool([str(Path(sys.argv[1]).resolve())],size=1)
live={};teams={};actions=[];children=[];indicators=[];deaths=[];observed={}
class Enough(Exception):pass
def spawn(i,init):
 teams[i]=next(s.split()[1] for s in init.decode().splitlines() if s.startswith('TEAM '))
 if teams[i]=='A':live[i]=Bot(pool,init=init,name=str(i));children.append(i)
def reply(i,block):
 rnd=int(block.split()[1]);
 if rnd>=2:raise Enough()
 if teams[i]=='B':return b'MOVE E\nENDTURN\n'
 altered=re.sub(rb'^ROUND \d+',f'ROUND {rnd+410}'.encode(),block)
 out=live[i].ask(altered)
 assert not live[i].error,live[i].error
 # A queen's newborn tail gift deliberately outputs no MOVE/SPLIT.
 # Only that two-segment child may die; the fixed queen must remain alive.
 act=next((x for x in out.decode().splitlines() if x.startswith(('MOVE ','SPLIT '))), 'NO_ACTION')
 observed[i]=int(re.search(rb'LENGTH (\d+)',block).group(1))
 actions.append((rnd,i,act))
 indicators.extend((rnd,i,line) for line in out.decode().splitlines() if line.startswith("INDICATOR "))
 return out
def death(i,r,reason):
 deaths.append((i,r,reason))
 bot=live.pop(i,None)
 if bot:bot.stop()
try:
 try:engine.run(('\n'.join(lines)+'\n').encode(),reply,death,spawn,lambda s:None,seed=101)
 except Enough:pass
finally:
 for bot in live.values():bot.stop()
 pool.close()
print('actions',actions[:10],'children',children)
assert (0,0,'SPLIT 2') in actions,actions
assert any(r==0 and i>1 and a=='NO_ACTION' for r,i,a in actions),actions
assert any(r==1 and i==0 and a.startswith('MOVE ') for r,i,a in actions),actions
assert any('QUEEN_TAIL_RELEASE' in line for _,_,line in indicators),indicators
assert any('QUEEN_TAIL_GIFT_2' in line for _,_,line in indicators),indicators
assert len(deaths)==1 and deaths[0][0]>1 and deaths[0][1:]==(0,'A'),deaths
assert observed[0]==8 and all(size==2 for i,size in observed.items() if i>1),observed
print('PASS: tail-blocked length10 queen retained 8; newborn2 died in place; original queen moved next round')
