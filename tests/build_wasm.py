"""Build the submission with the judge compiler, including headers in the cache key.
unswbc 1.2.3's clangtool cache only hashes C/C++ translation units.
"""
import argparse,hashlib,shutil,tempfile
from pathlib import Path
from unswbc.clangtool import build

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('project',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args()
files=sorted((args.project/'src').glob('*'))
files=[p for p in files if p.suffix in {'.cpp','.hpp'}]
assert len([p for p in files if p.suffix=='.cpp'])==1
fingerprint=hashlib.sha256(b''.join(p.name.encode()+p.read_bytes() for p in files)).hexdigest()[:24]
with tempfile.TemporaryDirectory(prefix='bc-wasm-') as temporary:
    staging=Path(temporary)/('submission-'+fingerprint)
    (staging/'src').mkdir(parents=True)
    for p in files:shutil.copyfile(p,staging/'src'/p.name)
    shutil.copyfile(args.project/'bot.toml',staging/'bot.toml')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(build(staging),args.output)
print('Verified submission build:',args.output,'header fingerprint:',fingerprint)
