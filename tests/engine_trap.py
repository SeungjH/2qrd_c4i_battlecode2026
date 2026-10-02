from pathlib import Path
from unswbc.engine import EngineModule
from unswbc.bot import Pool,Bot
from collections import Counter
import re,json,sys
w=h=20;walls=set()
for x in range(8,17):walls|={('H',x,10),('H',x,11)}
walls.add(('V',17,10))
lines=[f'MAP {w} {h}','MAP_NAME Courier Integration',f'TILE_COUNT {w*h}']
for y in range(h):
 for x in range(w):
  food=y==10 and 8<=x<=16;lines.append(f'TILE {x} {y} {int(food)} {int(food)}')
edges=[]
for r in range(2*h+1):
 for x in range(w+(r%2)):
  wall=('H' if r%2==0 else 'V',x%w,(r//2)%h) in walls;edges.append(f'EDGE {r*(w+1)+x} {int(wall)} -1')
lines+=[f'EDGE_COUNT {len(edges)}',*edges,'DRAGON_COUNT 3','DRAGON 0 2 2 16 1 16','DRAGON 1 2 2 2 1 2','DRAGON 0 3 7 10 6 10 5 10','END']
mp='\n'.join(lines)+'\n'
engine=EngineModule();pool=Pool([str(Path(sys.argv[1]).resolve())],size=1);live={};teams={};counts=Counter();sent=set();deliveries=[];indicators=[]
def spawn(i,init):
 t=next(l.split()[1] for l in init.decode().splitlines() if l.startswith('TEAM'));teams[i]=t
 if t=='A':live[i]=Bot(pool,init=init,name=str(i))
def reply(i,block):
 text=block.decode();rnd=int(text.split()[1]);facing=next(l.split()[1] for l in text.splitlines() if l.startswith('DIR '))
 if rnd>=22:return f'MOVE {dict(N="S",S="N",E="W",W="E")[facing]}\nENDTURN\n'.encode()
 if teams[i]=='B':return b'MOVE E\nENDTURN\n'
 text=re.sub(r'^ROUND \d+',f'ROUND {rnd+350}',text)
 for raw in sent:
  if str(raw) in text:deliveries.append((i,rnd))
 out=live[i].ask(text.encode())
 assert not live[i].error,live[i].error
 for l in out.decode().splitlines():
  if l.startswith('SONAR '):
   raw=int(l.split()[-1]);typ=(raw>>52)&7
   if raw>>56==0xD7 and typ==2:sent.add(raw);counts['escape_packets']+=1
   if raw>>56==0xD7 and typ==1:counts['warning_packets']+=1
  if l.startswith('INDICATOR ') and teams[i]=='A':indicators.append((i,rnd,l))
 return out
def death(i,r,reason):
 b=live.pop(i,None)
 if b:b.stop()
try:
 res=engine.run(mp.encode(),reply,death,spawn,lambda s:None,seed=2)
 engine.replay('v3 courier test','straight-line fixture')
finally:
 for b in live.values():b.stop()
 pool.close()
print('counts',dict(counts),'newborn deliveries',deliveries)

assert sent and deliveries and counts['warning_packets']>0
assert any(i>1 and 'COURIER_ESCAPE' in s for i,r,s in indicators)
print('PASS: official engine delivered split-turn mission to newborn; child relayed entrance warning')
