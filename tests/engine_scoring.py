from unswbc.engine import EngineModule
W,H=30,20
lines=[f'MAP {W} {H}','MAP_NAME Score Probe',f'TILE_COUNT {W*H}']
for y in range(H):
 for x in range(W):lines.append(f'TILE {x} {y} 0 0')
count=(2*H+1)*(W+1)
lines.append(f'EDGE_COUNT {count}')
for k in range(count):lines.append(f'EDGE {k} 0 -1')
lines+=['DRAGON_COUNT 3','DRAGON 0 4 5 5 4 5 3 5 2 5',
        'DRAGON 1 6 20 10 19 10 18 10 17 10 16 10 15 10',
        'DRAGON 0 20 '+' '.join(f'{(25-k)%30} 15' for k in range(20)),'END']
r=EngineModule().run(('\n'.join(lines)+'\n').encode(),
  lambda i,block:b'MOVE E\nENDTURN\n',lambda *a:None,lambda *a:None,lambda s:None,seed=1)
assert r.a_longest==20 and r.b_longest==6 and r.a_queen==4 and r.b_queen==6 and r.winner=='B',r
print('PASS: fixed queen 6 beats queen 4 even against a surviving length-20 collector')
