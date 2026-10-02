"""Queen keeps its length through an open exit beside an allied head."""
from pathlib import Path
import sys
from unswbc.bot import Bot, Pool
from unswbc.engine import EngineModule

width = height = 20
body = [(10, 10), (9, 10), (8, 10), (8, 11), (8, 12), (7, 12)]
lines = [f'MAP {width} {height}', 'MAP_NAME Royal Escape',
         f'TILE_COUNT {width * height}']
lines += [f'TILE {x} {y} 0 0' for y in range(height) for x in range(width)]
edges = []
for row in range(2 * height + 1):
    for x in range(width + row % 2):
        blocked = (row == 20 and x == 10) or (row == 21 and x == 11)
        edges.append(f'EDGE {row * (width + 1) + x} {int(blocked)} -1')
lines += [f'EDGE_COUNT {len(edges)}', *edges, 'DRAGON_COUNT 3',
          'DRAGON 0 6 ' + ' '.join(f'{x} {y}' for x, y in body),
          'DRAGON 1 2 2 2 2 3', 'DRAGON 0 2 11 11 12 11', 'END']

pool = Pool([str(Path(sys.argv[1]).resolve())], size=1)
queen = None
first_action = None
next_length = None

class Complete(Exception):
    pass

def spawn(owner, init):
    global queen
    if owner == 0:
        queen = Bot(pool, init=init, name='queen')

def reply(owner, block):
    global first_action, next_length
    if owner != 0:
        return b'MOVE E\nENDTURN\n'
    fields = block.decode().split()
    if int(fields[fields.index('ROUND') + 1]) > 0:
        next_length = int(fields[fields.index('LENGTH') + 1])
        raise Complete()
    output = queen.ask(block)
    assert not queen.error, queen.error
    first_action = next(line for line in output.decode().splitlines()
                        if line.startswith(('MOVE ', 'SPLIT ')))
    return output

try:
    try:
        EngineModule().run(('\n'.join(lines) + '\n').encode(), reply,
                           lambda *_: None, spawn, lambda _: None, seed=101)
    except Complete:
        pass
finally:
    if queen:
        queen.stop()
    pool.close()

assert first_action == 'MOVE S' and next_length == 6, (first_action, next_length)
print('PASS: real engine queen takes the allied escape and retains all six segments')
