"""Parallel Battlecode comparison for native executables or judge WASM artifacts."""
from __future__ import annotations
import argparse
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import asdict
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


def match(job: tuple[str, int, str, str, str, str | None]) -> dict:
    map_path, seed, side, candidate, baseline, replay_dir = job
    from unswbc.engine import EngineModule
    from unswbc.bot import Pool, Bot

    bins = {'A': candidate if side == 'A' else baseline,
            'B': candidate if side == 'B' else baseline}
    engine = EngineModule()
    wasm = candidate.endswith('.wasm')
    if wasm:
        from unswbc.sandbox import WasmPool,SandboxBot
        Pool,Bot=WasmPool,SandboxBot
    pools = {t: Pool([b], size=1, key=f'{seed:016x}-{t.lower()}') for t, b in bins.items()}
    max_points={'A':0,'B':0};max_memory={'A':0,'B':0};turns={'A':0,'B':0}
    live: dict[int, Bot] = {}
    queen_deaths = {}
    stats = {'candidate':{'donations':0,'clearances':0,'body_releases':0},'baseline':{'donations':0,'clearances':0,'body_releases':0}}
    teams: dict[int, str] = {}
    started = time.monotonic()

    def spawn(i: int, init: bytes) -> None:
        team = next(line.split()[1] for line in init.decode().splitlines()
                    if line.startswith('TEAM '))
        teams[i] = team
        live[i] = Bot(pools[team], init=init, name=str(i))

    def reply(i: int, block: bytes) -> bytes:
        bot = live[i]
        out = bot.ask(block)
        if bot.error:
            raise RuntimeError(f'bot {i} ({teams[i]}): {bot.error}')
        metrics=getattr(bot,'live',None)
        if metrics and metrics[0]:
            max_points[teams[i]]=max(max_points[teams[i]],metrics[0])
            max_memory[teams[i]]=max(max_memory[teams[i]],metrics[1])
        turns[teams[i]]+=1
        owner='candidate' if teams[i]==side else 'baseline'
        stats[owner]['donations'] += int(b'DONATE_' in out)
        stats[owner]['clearances'] += int(b'QUEEN_CLEAR_' in out)
        stats[owner]['body_releases'] += int(b'QUEEN_BODY_RELEASE' in out or b'QUEEN_TAIL_RELEASE' in out)
        return out

    def death(i: int, _round: int, _reason: str) -> None:
        if i<=1: queen_deaths[teams[i]]=_round
        bot = live.pop(i, None)
        if bot:
            bot.stop()

    try:
        result = engine.run(Path(map_path).read_bytes(), reply, death,
                            spawn, lambda _line: None, seed=seed)
        if replay_dir:
            path = Path(replay_dir) / f'{Path(map_path).stem}_{seed}_{side}.replay'
            path.write_bytes(engine.replay('candidate' if side == 'A' else 'baseline',
                                           'candidate' if side == 'B' else 'baseline'))
    finally:
        for bot in live.values():
            bot.stop()
        for pool in pools.values():
            pool.close()
    return {'map': Path(map_path).stem, 'seed': seed, 'candidate_side': side,
            'candidate_won': result.winner == side,
            'draw':result.winner not in {'A','B'},
            'candidate_queen_death':queen_deaths.get(side),
            'baseline_queen_death':queen_deaths.get('B' if side=='A' else 'A'),
            'actions':stats,'max_points':max_points,'max_memory':max_memory,'turns':turns,
            'result': asdict(result),
            'elapsed_seconds': round(time.monotonic() - started, 3)}


def prepare_bot(source: Path, destination: Path) -> str:
    if source.suffix not in {'.cc', '.cpp', '.cxx'}:
        return str(source.resolve())
    subprocess.run(['clang++', '-std=c++20', '-O2', str(source.resolve()),
                    '-o', str(destination)], check=True)
    return str(destination)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True,
                        help='C++ source or precompiled native executable')
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--maps', nargs='+', type=Path, required=True)
    parser.add_argument('--seeds', nargs='+', type=int, required=True)
    parser.add_argument('--workers', type=int, default=min(8, max(1, (os.cpu_count() or 2) - 2)))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--replay-dir', type=Path,
                        help='Write replays when needed; omitted for fast runs')
    args = parser.parse_args()
    if args.workers < 1 or not args.maps or not args.seeds:
        parser.error('workers, maps and seeds must be nonempty and positive')
    for path in [args.candidate, args.baseline, *args.maps]:
        if not path.is_file():
            parser.error(f'file not found: {path}')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.replay_dir:
        args.replay_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='bc-parallel-') as temporary:
        tmp = Path(temporary)
        candidate = prepare_bot(args.candidate, tmp / 'candidate')
        baseline = prepare_bot(args.baseline, tmp / 'baseline')
        jobs = [(str(map_path.resolve()), seed, side, candidate, baseline,
                 str(args.replay_dir.resolve()) if args.replay_dir else None)
                for map_path in args.maps for seed in args.seeds for side in 'AB']
        started = time.monotonic()
        results = []
        with ProcessPoolExecutor(max_workers=min(args.workers, len(jobs))) as executor:
            futures = {executor.submit(match, job): job for job in jobs}
            for future in as_completed(futures):
                result = future.result()
                results.append(result)
                wins = sum(row['candidate_won'] for row in results)
                print(f"{len(results)}/{len(jobs)} {result['map']} seed={result['seed']} "
                      f"side={result['candidate_side']} "
                      f"{'WIN' if result['candidate_won'] else 'LOSS'} "
                      f"({wins}/{len(results)})", flush=True)
        wall = round(time.monotonic() - started, 3)
        results.sort(key=lambda row: (row['map'], row['seed'], row['candidate_side']))
        summary = {'candidate': str(args.candidate.resolve()),
                   'baseline': str(args.baseline.resolve()),
                   'workers': min(args.workers, len(jobs)), 'games': len(results),
                   'wins': sum(row['candidate_won'] for row in results),
                   'draws':sum(row['draw'] for row in results),
                   'wall_seconds': wall,
                   'sum_match_seconds': round(sum(row['elapsed_seconds'] for row in results), 3),
                   'results': results}
        args.output.write_text(json.dumps(summary, indent=2) + '\n')
        print(f"Finished {summary['wins']}/{len(results)} in {wall:.1f}s; "
              f"saved {args.output}")


if __name__ == '__main__':
    main()
