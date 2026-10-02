"""A learned blind alley must lose only the original two-segment head."""
from pathlib import Path
import re, sys
from unswbc.engine import EngineModule
from unswbc.bot import Pool, Bot

for length in (4, 6):
    w,h=32,20
    stop=10+length-3
    walls={2*10*(w+1)+x for x in range(8,stop+1)}
    walls|={2*11*(w+1)+x for x in range(8,stop+1)}
    walls.add((2*10+1)*(w+1)+stop+1)
    lines=[f'MAP {w} {h}','MAP_NAME Reverse discard',f'TILE_COUNT {w*h}']
    lines += [f'TILE {x} {y} 0 0' for y in range(h) for x in range(w)]
    edges=[f'EDGE {r*(w+1)+x} {int(r*(w+1)+x in walls)} -1' for r in range(2*h+1) for x in range(w+r%2)]
    lines += [f'EDGE_COUNT {len(edges)}',*edges,'DRAGON_COUNT 3',
              'DRAGON 0 2 2 2 1 2','DRAGON 1 2 22 16 21 16',
              f'DRAGON 0 {length} '+ ' '.join(f'{10-k} 10' for k in range(length)), 'END']
    pool=Pool([str(Path(sys.argv[1]).resolve())],size=1)
    live={};actions=[];deaths=[];observed={}
    class Enough(Exception):pass
    def spawn(i,init):
        if i>=2:live[i]=Bot(pool,init=init,name=str(i))
    def reply(i,block):
        rnd=int(block.split()[1])
        if rnd>=length+1:raise Enough()
        if i<2:return b'MOVE E\nENDTURN\n'
        observed[i]=int(re.search(rb'LENGTH (\d+)',block).group(1))
        out=live[i].ask(re.sub(rb'^ROUND \d+',f'ROUND {400+rnd}'.encode(),block))
        assert not live[i].error,live[i].error
        actions.append((i,rnd,out.decode()))
        return out
    def death(i,r,reason):
        deaths.append((i,r,reason))
        b=live.pop(i,None)
        if b:b.stop()
    try:
        try:EngineModule().run(('\n'.join(lines)+'\n').encode(),reply,death,spawn,lambda _:None,seed=101)
        except Enough:pass
        assert any(i==2 and f'SPLIT {length-2}\n' in out for i,_,out in actions),actions
        assert any(i==2 and 'DISCARD_TRAPPED_HEAD_2' in out for i,_,out in actions),actions
        assert [i for i,_,_ in deaths]==[2],deaths
        assert observed[2]==2 and observed[3]==length-2 and 3 in live,(observed,deaths)
        assert not any(i==3 and 'SPLIT ' in out for i,_,out in actions),actions
        print(f'PASS: length {length}: old head 2 died; reversed child {length-2} survived without re-splitting')
    finally:
        for b in live.values():b.stop()
        pool.close()
