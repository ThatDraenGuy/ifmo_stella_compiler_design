import subprocess
import time
import json
import csv
from itertools import product
from pathlib import Path
from typing import Dict, List, Tuple

steps_range = [1, 3, 10, 100]
threshold_range = [1.0, 0.8, 0.5, 0.25]
bytes_range = [2048, 4096, 8192]

# programs = [{ id: "garbage", input: 30 }]
programs = {
    "chat": {
        "input": "10"
    },
    "double": {
        "input": "20"
    },
    "garbage": {
        "input": "30"
    }
}

runs_count = 5

build_dir = Path("build").absolute()
run_dir = Path("run")
target_name = "stellac_runtime_app"

class ExecResult:
    exec_time: float
    return_code: int
    stats: Dict[str, str]
    failure_reason: str | None
    def __init__(self, exec_time: float, return_code: int, stats: Dict[str, str], failure_reason: str | None):
        self.exec_time = exec_time
        self.return_code = return_code
        self.stats = stats
        self.failure_reason = failure_reason


def build_program(program_id: str):
    program_path = Path(f"programs/{program_id}.stella").absolute()
    subprocess.run(["cmake", f"-DUSER_SRC_ST={program_path}", "../..", "-B", build_dir])
    subprocess.run(["cmake", "--build", build_dir, "--target", target_name])
    subprocess.run(["cp", build_dir.joinpath("runtime", target_name), run_dir.joinpath(program_id)])

def run_program(program_id: str, env: Dict[str, str], args: List[str]) -> ExecResult:
    start = time.time()
    res = subprocess.run([
        run_dir.joinpath(program_id),
        *args
    ], env=env, capture_output=True, text=True)
    exec_time = time.time() - start
    if res.returncode == 0:
        stats = json.loads(res.stderr)
        return ExecResult(exec_time, res.returncode, stats, None)
    else:
        return ExecResult(exec_time, res.returncode, {}, res.stderr)


def run_program_times(program_id: str, env: Dict[str, str], args: List[str]) -> ExecResult:
    results = [run_program(program_id, env, args) for i in range(runs_count)]
    exec_times = [res.exec_time for res in results]
    return ExecResult(sum(exec_times) / len(exec_times), results[0].return_code, results[0].stats, results[0].failure_reason)

def get_env(steps: int, threshold: float) -> Dict[str, str]:
    return {
        "STELLA_GC_STATS": "1",
        "STELLA_GC_STATS_JSON": "1",
        "STELLA_GC_COLLECT_STEPS_COUNT": str(steps),
        "STELLA_GC_COLLECT_START_THRESHOLD": str(threshold)
    }

def get_args(program_id: str, heap_size: int) -> List[str]:
    return [programs[program_id]["input"], str(heap_size)]

def run_program_params(program_id: str) -> List[Tuple[int, float, int, ExecResult]]:
    build_program(program_id)
    params = product(bytes_range, threshold_range, steps_range)
    return [(tuple[0], tuple[1], tuple[2], run_program_times(program_id, get_env(tuple[2], tuple[1]), get_args(program_id, tuple[0]))) for tuple in params]

def run_all_programs() -> Dict[str, List[Tuple[int, float, int, ExecResult]]]:
    return {id: run_program_params(id) for id in programs.keys()}


def map_result_to_csv(res_and_params: Tuple[int, float, int, ExecResult]) -> Dict[str, str]:
    res = res_and_params[3]
    if res.return_code == 0:
        return {
            "heap_size": str(res_and_params[0]),
            "start_collect_threshold": str(res_and_params[1]),
            "collection_steps_count": str(res_and_params[2]),
            "collections_count": res.stats["collections"],
            "read_barrier_activations_count": res.stats["read_barrier_activations"],
            "max_occupied_bytes": res.stats["maximum_occupied_bytes"],
            "exec_time": str(res.exec_time),
            "failure_reason": ""
        }
    else:
        return {
            "heap_size": str(res_and_params[0]),
            "start_collect_threshold": str(res_and_params[1]),
            "collection_steps_count": str(res_and_params[2]),
            "collections_count": "",
            "read_barrier_activations_count": "",
            "max_occupied_bytes": "",
            "exec_time": str(res.exec_time),
            "failure_reason": str(res.failure_reason)
        }

def run_and_record():
    results = run_all_programs()
    for id in results.keys():
        res_list = results[id]
        csv_data = [map_result_to_csv(res) for res in res_list]
        fieldnames = csv_data[0].keys()
        with open(f"output/{id}.csv", mode="w", newline="", encoding="utf-8") as file:
            writer = csv.DictWriter(file, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(csv_data)

run_and_record()

