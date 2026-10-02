"""The engine must execute the donor's two-step action without spending length."""
from pathlib import Path
import re,sys
from unswbc.engine import EngineModule
from unswbc.bot import Pool,Bot
w,h=32,20
lines=[f'MAP {w} {h}','MAP_NAME Sprint Regression',f'TILE_COUNT {w*h}']
lines += [f'TILE {x} {y} 0 0' for y in range(h) for x in range(w)]
edges=[f'EDGE {r*(w+1)+x} 0 -1' for r in range(2*h+1) for x in range(w+r%2)]
lines += [f'EDGE_COUNT {len(edges)}',*edges,'DRAGON_COUNT 3',
          'DRAGON 0 8 18 10 18 11 18 12 19 12 20 12 20 11 20 10 20 9',
          'DRAGON 1 2 25 16 24 16',
          'DRAGON 0 6 10 10 9 10 8 10 8 11 9 11 9 12','END']
engine=EngineModule();pool=Pool([str(Path(sys.argv[1]).resolve())],size=1);live={};queen=(18,10);moves=[]
class Enough(Exception):pass
def spawn(i,init):
 if i==2:live[i]=Bot(pool,init=init,name=str(i))
def reply(i,block):
 global queen
 rnd=int(block.split()[1])
 if i==0:queen=(queen[0],(queen[1]-1)%h);return b'MOVE N\nENDTURN\n'
 if i==1:return b'MOVE E\nENDTURN\n'
 fields=block.split();length=int(fields[fields.index(b'LENGTH')+1]);tile=block.decode().splitlines()[1+24].split()
 # ECHOES occupies the first line after the header in current toolkit.
 text=block.decode();rows=text.splitlines();first=next(k for k,l in enumerate(rows) if re.match(r'^\d+ \d+ [01] ',l))
 actual=tuple(map(int,rows[first+24].split()[:2]))
 if rnd==1:
  dirs=moves[0];expected=[10,10]
  for d in dirs:
   dx,dy={'N':(0,-1),'E':(1,0),'S':(0,1),'W':(-1,0)}[d];expected=[(expected[0]+dx)%w,(expected[1]+dy)%h]
  assert actual==tuple(expected) and length==6,(actual,expected,length)
  raise Enough()
 shifted=re.sub(rb'^ROUND \d+',b'ROUND 360',block)
 raw=(0xD7<<56)|(7<<52)|(360<<27)|(queen[1]*64+queen[0])|(8<<12)
 count=int(re.search(rb'NUM_MSGS (\d+)',shifted).group(1))
 shifted=re.sub(rb'NUM_MSGS \d+',f'NUM_MSGS {count+1}\n{raw}'.encode(),shifted,count=1)
 out=live[i].ask(shifted);assert not live[i].error,live[i].error
 action=next(l for l in out.decode().splitlines() if l.startswith('MOVE '));dirs=action.split()[1]
 assert len(dirs)==2 and 'DONOR_SPRINT' in out.decode(),out
 moves.append(dirs);return out
def death(i,*_):
 b=live.pop(i,None)
 if b:b.stop()
try:
 try:engine.run(('\n'.join(lines)+'\n').encode(),reply,death,spawn,lambda _:None,seed=101)
 except Enough:pass
finally:
 for b in live.values():b.stop()
 pool.close()
assert moves
print('PASS: official engine executed',moves[0],'and retained length 6')
