"""Multiple autonomous donors must feed the fixed queen, with no initial pearls.
Uses the official engine and production bot for every friendly dragon.
"""
from pathlib import Path
import re,sys
from unswbc.engine import EngineModule
from unswbc.bot import Bot,Pool
w,h=32,32
lines=[f'MAP {w} {h}','MAP_NAME Feeding Regression',f'TILE_COUNT {w*h}']
lines += [f'TILE {x} {y} 0 0' for y in range(h) for x in range(w)]
edges=[f'EDGE {r*(w+1)+x} 0 -1' for r in range(2*h+1) for x in range(w+r%2)]
bodies=[(0,[(10,12),(9,12),(8,12),(7,12),(6,12),(6,13),(7,13),(8,13)]),
        (1,[(25,20),(24,20)]),
        (0,[(13,11),(14,11)]),(0,[(11,8),(11,7)]),
        (0,[(15,15),(16,15),(16,16),(15,16)]),
        (0,[(6,9),(5,9)]),(0,[(12,16),(12,17)]),
        (0,[(17,12),(18,12),(18,13),(18,14),(17,14),(16,14)])]
certified='--certified-fallback' in sys.argv
fallback='--fallback' in sys.argv
large='--large' in sys.argv  # Diagnostic only: crowded length-16 donor remains a known failing case.
friendly=[b for t,b in bodies if t==0]
if large:
 friendly[1:]=[[(6,9),(5,9)],[(8,8),(8,7)],[(6,16),(5,16),(5,17),(6,17)],
               [(14,16),(14,17)],[(12,16),(12,17)],
               [(12,11),(13,11),(14,11),(15,11),(15,10),(14,10),(13,10),(12,10),
                (12,9),(13,9),(14,9),(15,9),(15,8),(14,8),(13,8),(12,8)]]
if fallback:
 friendly=[[(2,2),(1,2)],friendly[0][:4],*friendly[1:-1],friendly[-1][:2]]
if certified:
 friendly=[[(10,10),(9,10),(8,10),(7,10)],
           [(13,11),(14,11),(15,11),(15,12),(14,12),(13,12),(12,12),(12,11)],
           [(17,10),(18,10)],[(11,15),(11,16)],[(7,14),(6,14)]]
bodies=[]
for k,b in enumerate(friendly):bodies.extend([(0,b),(1,[(25,24+k),(24,24+k)])])
lines += [f'EDGE_COUNT {len(edges)}',*edges,f'DRAGON_COUNT {len(bodies)}']
lines += ['DRAGON '+str(t)+' '+str(len(b))+' '+' '.join(f'{x} {y}' for x,y in b) for t,b in bodies]
lines+=['END'];mapbytes=('\n'.join(lines)+'\n').encode()
results=[]
for seed in (101,202,303):
 trial=[]
 for line in lines:
  fields=line.split()
  if fields[0]=='DRAGON':
   coords=list(map(int,fields[3:]))
   for k in range(0,len(coords),2):
    if seed==202:coords[k]=w-1-coords[k]
    if seed==303:coords[k+1]=h-1-coords[k+1]
   line=' '.join(fields[:3]+list(map(str,coords)))
  trial.append(line)
 mapbytes=('\n'.join(trial)+'\n').encode()
 engine=EngineModule();pool=Pool([str(Path(sys.argv[1]).resolve())],size=1)
 live={};teams={};indicators={};lengths=[];gifts=[];queen_dead=[];observed={};drops={};recovered=[];violations=[]
 def spawn(i,init):
  teams[i]=next(s.split()[1] for s in init.decode().splitlines() if s.startswith('TEAM '))
  if teams[i]=='A':live[i]=Bot(pool,init=init,name=str(i))
 def reply(i,block):
  rnd=int(block.split()[1])
  text=block.decode();rows=text.splitlines();start=next(k for k,l in enumerate(rows) if re.match(r'^\d+ \d+ [01] ',l))
  cells={tuple(map(int,row.split()[:2])):int(row.split()[2]) for row in rows[start:start+49]}
  head=tuple(map(int,rows[start+24].split()[:2]));n=int(rows[start+49].split()[1]);parts={}
  for row in rows[start+50:start+50+n]:
   f=row.split()
   if int(f[1])==i:parts[(int(f[2]),int(f[3]))]=f[4]
  b=[head]
  while len(b)<len(parts):
   previous=next((p for p,d in parts.items() if p not in b and ((p[0]+{'N':0,'E':1,'S':0,'W':-1}[d])%w,(p[1]+{'N':-1,'E':0,'S':1,'W':0}[d])%h)==b[-1]),None)
   if previous is None:break
   b.append(previous)
  observed[i]=b

  if certified and i==0:return (b'MOVE W\nENDTURN\n' if seed==202 else b'MOVE E\nENDTURN\n') if rnd==0 else b'ENDTURN\n'
  if fallback and i<=1:return b'MOVE E\nENDTURN\n' if seed==202 else b'MOVE W\nENDTURN\n'
  if teams[i]=='B':return b'MOVE W\nENDTURN\n' if seed==202 else b'MOVE E\nENDTURN\n'
  if i==(2 if fallback or certified else 0):lengths.append(int(re.search(rb'LENGTH (\d+)',block).group(1)))
  out=live[i].ask(re.sub(rb'^ROUND \d+',f'ROUND {min(rnd+350,499)}'.encode(),block))
  assert not live[i].error,live[i].error
  indicators[i]=next((s for s in out.decode().splitlines() if s.startswith('INDICATOR ')),'')
  action=next((l for l in out.decode().splitlines() if l.startswith(('MOVE ','SPLIT '))),'')
  if action.startswith('MOVE '):
   pos=head
   for d in action.split()[1]:
    dx,dy={'N':(0,-1),'E':(1,0),'S':(0,1),'W':(-1,0)}[d];pos=((pos[0]+dx)%w,(pos[1]+dy)%h)
    if cells.get(pos) and pos in drops:
     receiver=drops.pop(pos)
     if i!=receiver:violations.append((rnd,i,receiver,pos))
     recovered.append(i)
  return out
 def death(i,rnd,reason):
  if i==(2 if fallback or certified else 0):queen_dead.append((rnd,reason))
  if 'DONATE_' in indicators.get(i,''):
   gifts.append((rnd,i,indicators[i]));receiver=int(re.search(r'peer=(\d+)',indicators[i]).group(1))
   for pos in observed.get(i,[])[::2]:drops[pos]=receiver
  b=live.pop(i,None)
  if b:b.stop()
 try:
  engine.run(mapbytes,reply,death,spawn,lambda _:None,seed=seed)
  if len(sys.argv)>2 and not sys.argv[2].startswith('--'):Path(sys.argv[2]+f'_{seed}.replay').write_bytes(engine.replay('feeding','scripted opponent'))
 finally:
  for b in live.values():b.stop()
  pool.close()
 row={'seed':seed,'gifts':len(gifts),'queen_max':max(lengths,default=0),'queen_last':lengths[-1] if lengths else 0,'queen_deaths':queen_dead,'verified_gift_meals':len(recovered),'violations':violations}
 results.append(row);print(row,flush=True)
 assert not queen_dead and not violations,row
 if certified:
  assert len(gifts)>=2 and len(recovered)>=2 and lengths[-1]>=10,row
 elif fallback:
  assert not gifts and not recovered,row # An unseen death cannot authorize a fallback sacrifice.
 else:
  assert len(gifts)>=3 and len(recovered)>=(10 if large else 6),row
  assert lengths[-1]>=12,row
print('PASS: unseen queen death does not authorize fallback gifts' if fallback else 'PASS: multiple gifts recovered by the designated receiver, with no initial pearls and verified exclusive gift recovery')
