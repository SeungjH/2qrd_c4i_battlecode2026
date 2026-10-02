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
live={};teams={};actions=[];children=[];indicators=[]
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
 act=next(x for x in out.decode().splitlines() if x.startswith(('MOVE ','SPLIT ')))
 actions.append((rnd,i,act))
 indicators.extend((rnd,i,line) for line in out.decode().splitlines() if line.startswith("INDICATOR "))
 return out
def death(i,r,reason):
 bot=live.pop(i,None)
 if bot:bot.stop()
try:
 try:engine.run(('\n'.join(lines)+'\n').encode(),reply,death,spawn,lambda s:None,seed=101)
 except Enough:pass
finally:
 for bot in live.values():bot.stop()
 pool.close()
print('actions',actions[:10],'children',children)
assert (0,0,'SPLIT 8') in actions
assert any(i>1 and a.startswith('MOVE ') for r,i,a in actions if r==0)
assert any(r==0 and i>1 and 'official=0 ' in line and 'queen=0 ' in line for r,i,line in indicators), indicators
print('PASS: official engine created the eight-segment reversed child and it moved in the split round')
