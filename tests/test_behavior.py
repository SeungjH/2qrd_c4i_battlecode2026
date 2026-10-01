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
        if actions[0].startswith('MOVE '):assert len(actions[0].split()[1])==1,actions
        else:assert actions[0]=='SPLIT 2',actions
        return actions[0],out
    def close(self): self.p.stdin.close();self.p.wait(timeout=2)

def once(body,owner=0,**kw):
    c=Client(owner);out=c.ask(frame(10,body,owner,echo=False,**kw));c.close();return out

# Directional exploration must leave a single toroidal row, without short loops.
c=Client();body=[(5,5),(4,5),(3,5)];seen=set(body[:1]);turns=0;previous='E'
for r in range(200):
    action,out=c.ask(frame(r,body,echo=r>0))
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
a,_=once([(10,10),(9,10),(8,10),(7,10),(6,10),(5,10),(4,10)])
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
print('ALL BEHAVIOR REGRESSIONS PASSED')
