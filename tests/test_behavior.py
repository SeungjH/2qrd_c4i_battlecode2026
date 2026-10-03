from pathlib import Path
import subprocess
import sys

BIN=str(Path(sys.argv[1]).resolve())
DIR='NESW'; OFF=[(0,-1),(1,0),(0,1),(-1,0)]

def facing(a,b,w,h):
    for i,(dx,dy) in enumerate(OFF):
        if ((b[0]+dx)%w,(b[1]+dy)%h)==a:return DIR[i]
    raise ValueError((a,b))

def frame(round_no,body,owner=0,others=(),pearls=(),walls=(),w=20,h=20,echo=True):
    x,y=body[0]
    lines=[f'ROUND {round_no}',f'DIR {facing(body[0],body[1],w,h)}',f'LENGTH {len(body)}',f'UNIT_COUNT {1+len(others)}','NUM_MSGS 0']
    if echo:lines.append('ECHOES 0 0 0 0 0')
    visible={((x+dx)%w,(y+dy)%h) for dy in range(-3,4) for dx in range(-3,4)}
    for dy in range(-3,4):
        for dx in range(-3,4):
            p=((x+dx)%w,(y+dy)%h); lines.append(f'{p[0]} {p[1]} {int(p in pearls)} -1')
    parts=[]
    for o,b in [(owner,body),*others]:
        for k,p in enumerate(b):
            if p in visible:
                d=facing(b[0],b[1],w,h) if k==0 else facing(b[k-1],p,w,h)
                parts.append(f'A {o} {p[0]} {p[1]} {d} {int(k==0)}')
    lines.append(f'DRAGON_BODIES {len(parts)}');lines+=parts
    # Canonical walls: ('H', x, y) is the north edge of tile x,y.
    for r in range(8): lines.append(' '.join('w' if ('H',(x+c-3)%w,(y+r-3)%h) in walls else '.' for c in range(7)))
    for r in range(7): lines.append(' '.join('w' if ('V',(x+c-3)%w,(y+r-3)%h) in walls else '.' for c in range(8)))
    return '\n'.join(lines)+'\n'

class Client:
    def __init__(self,owner=0,w=20,h=20):
        self.p=subprocess.Popen([BIN],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
        self.p.stdin.write(f'ID {owner}\nTEAM A\nMAP {w} {h}\nUNIT_LIMIT 64\n');self.p.stdin.flush()
    def ask(self,block):
        self.p.stdin.write(block);self.p.stdin.flush();out=[]
        while True:
            line=self.p.stdout.readline().strip()
            assert line,'Bot exited'
            if line=='ENDTURN': break
            out.append(line)
        actions=[s for s in out if s.startswith(('MOVE ','SPLIT '))]
        assert len(actions)==1
        if actions[0].startswith('MOVE '):
            steps=actions[0].split()[1]
            assert len(steps)<=2,actions
            if len(steps)==2:
                assert int(block.split()[1])>=350 and int(block.split('LENGTH ')[1].split()[0])>=5
                assert any('DONOR_SPRINT' in s for s in out),out
        else:
            split_size=int(actions[0].split()[1])
            assert 2<=split_size<=int(block.split('LENGTH ')[1].split('\n')[0])-2,actions
        return actions[0],out
    def close(self): self.p.stdin.close();self.p.wait(timeout=2)

def once(body,owner=10,**kw):
    c=Client(owner);out=c.ask(frame(10,body,owner,echo=False,**kw));c.close();return out

# Directional exploration must leave a single toroidal row, without short loops.
c=Client(owner=10);body=[(5,5),(4,5),(3,5)];seen=set(body[:1]);turns=0;previous='E'
for r in range(200):
    action,out=c.ask(frame(r,body,owner=10,echo=r>0))
    assert action.startswith('MOVE ')
    d=action[-1];dx,dy=OFF[DIR.index(d)]
    p=((body[0][0]+dx)%20,(body[0][1]+dy)%20)
    assert p not in body,(r,body,p)
    turns+=d!=previous;previous=d
    body=[p]+body[:-1];seen.add(p)
c.close()
assert len(seen)>=150,(len(seen),turns)
print(f'empty exploration: {len(seen)} unique cells / 200 moves, {turns} turns')

# Every early split keeps all but exactly two segments on the parent.
a,_=once([(10,10),(9,10),(8,10),(8,9),(7,9),(7,10),(7,11)])
assert a=='SPLIT 2',a
print('routine length-7 split: child 2, parent retains 5')

# Child vacates the former penultimate cell so the parent can escape.
parent=[(5,5),(5,6),(6,6),(7,6),(7,5),(6,5),(6,4)]
walls={('H',5,5),('V',5,5)}
p=Client();a,_=p.ask(frame(10,parent,walls=walls,echo=False));assert a=='SPLIT 2',a
child=[parent[-1],parent[-2]];parent=parent[:-2]
c=Client(2);a,out=c.ask(frame(10,child,owner=2,others=[(0,parent)],walls=walls,echo=False))
assert a.startswith('MOVE '),(a,out)
dx,dy=OFF[DIR.index(a[-1])];child=[(child[0][0]+dx,child[0][1]+dy),child[0]]
a,_=p.ask(frame(11,parent,others=[(2,child)],walls=walls));assert a=='MOVE E',a
c.close();p.close();print('emergency split: parent retained 5 and escaped through released tail')

# If all child escape moves would retain the blocking tail by eating, lose only child 2.
child=[(6,4),(6,5)]
a,out=once(child,owner=2,others=[(0,parent)],walls=walls,pearls={(6,3),(7,4),(5,4)})
assert a=='MOVE S',(a,out)
assert any('SACRIFICE_TWO' in s for s in out)
print('blocked release: only 2-segment child sacrifices itself')

# No-exit fallback must never ram the ally head ahead.
a,_=once([(5,5),(5,6)],others=[(1,[(5,4),(5,3)])],walls={('V',5,5),('V',6,5)})
assert a=='MOVE S',a
print('no-exit fallback: avoids ally head')

# Forward preference yields to nearby allies.
a,_=once([(5,5),(5,6),(5,7)],others=[(1,[(5,3),(5,2),(5,1)])])
assert a in ('MOVE E','MOVE W'),a
print('ally separation: moves sideways away from ally ahead')

# Replay M840105: visible pearls lie behind a wall, six steps away by the north detour.
c=Client(owner=7,w=32,h=16)
walls={('V',18,y) for y in (12,13,14,15)}
a,out=c.ask(frame(185,[(19,13),(20,13)],owner=7,w=32,h=16,
                  pearls={(17,13),(17,14),(17,15)},walls=walls,echo=False))
assert a=='MOVE N',(a,out)
assert any('PEARL_ROUTE' in line for line in out),out
c.close();print('wall-detour pearl route: moves north toward reachable visible food')
print('ALL BEHAVIOR REGRESSIONS PASSED')

# Remote queen length report admits a free two-step donor move; post-action SONAR is accurate.
c=Client(owner=10,w=32,h=20)
b=[(10,10),(9,10),(8,10),(8,11),(9,11),(9,12)]
block=frame(360,b,owner=10,w=32,h=20,pearls={(11,10),(12,10)},echo=False)
raw=(0xD7<<56)|(7<<52)|(360<<27)|(10*64+18)|(8<<12)
block=block.replace('NUM_MSGS 0','NUM_MSGS 1\n'+str(raw))
a,out=c.ask(block)
assert a=='MOVE EE',(a,out)
statuses=[int(s.split()[2]) for s in out if s.startswith('SONAR ') and ((int(s.split()[2])>>52)&7)==0]
assert statuses and all((p&4095)==10*64+12 and ((p>>12)&4095)==8 for p in statuses),out
c.close();print('donor sprint: two starting-length free steps, two meals and correct final SONAR position/length')

# The fixed queen's safe home is the spawn position, not its later roaming position.
c=Client();b=[(5,5),(4,5),(3,5)]
for rnd in range(200):
 a,out=c.ask(frame(rnd,b,owner=0,echo=rnd>0))
 assert a.startswith('MOVE ') and len(a.split()[1])==1,(a,out)
 dx,dy=OFF[DIR.index(a[-1])];p=((b[0][0]+dx)%20,(b[0][1]+dy)%20)
 assert p not in b,(rnd,b,p)
 gap=max(min(abs(v-5),20-abs(v-5)) for v in p)
 assert gap<=4,(rnd,p,gap,out)
 b=[p]+b[:-1]
c.close();print('queen home: 200 safe turns within four cells of the spawn, no routine splitting or sprint')

# The same nearby recipient authorizes a fallback gift only after a queen death certificate.
receiver=[(12,10),(12,11),(12,12),(13,12),(13,11),(13,10),(13,9),(13,8)]
status=(0xD7<<56)|(77<<36)|(400<<27)|(10*64+12)|(8<<12)
death=(0xD7<<56)|(7<<52)|(398<<27)|(12*64+12)
for certified in (False,True):
    c=Client(owner=10)
    block=frame(400,[(10,10),(9,10)],owner=10,others=[(77,receiver)],echo=False)
    msgs=[status,death] if certified else [status]
    block=block.replace('NUM_MSGS 0',f'NUM_MSGS {len(msgs)}\n'+'\n'.join(map(str,msgs)))
    action,out=c.ask(block);c.close()
    sacrificed=any('DONATE_' in line for line in out)
    assert sacrificed==certified,(certified,action,out)
    if certified:assert action=='MOVE W',(action,out)
print('fallback gift: full bot refuses unknown queen state and sacrifices after a valid death certificate')

# Devil replay R220: the retained queen must re-check a newly opened safe exit.
c=Client(owner=0)
b=[(10,10),(10,11),(11,11),(12,11),(12,10),(12,9)]
walls={('H',10,10),('V',10,10),('V',11,10)}
a,out=c.ask(frame(400,b,owner=0,walls=walls,echo=False))
assert a=='SPLIT 4' and any('REVERSE_RESCUE' in line for line in out),(a,out)
walls.remove(('H',10,10))
a,out=c.ask(frame(401,b[:2],owner=0,walls=walls,others=[(2,[(13,9),(12,9),(12,10),(12,11)])]))
assert a=='MOVE N' and any('QUEEN_RECOVERED' in line for line in out),(a,out)
c.close();print('retained queen: opened safe exit cancels the old discard latch')
