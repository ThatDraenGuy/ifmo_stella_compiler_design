import subprocess
import time
import json
from pathlib import Path
from typing import Dict, List

steps_range = [1, 3, 10, 100]
threshold_range = [1.0, 0.8, 0.5, 0.25]
bytes_range = [2048, 4096, 8192]

programs = ["garbage"]

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
    subprocess.run(["--build", build_dir, "--target", target_name])
    subprocess.run(["cp", build_dir.joinpath("runtime", target_name), run_dir.joinpath(program_id)])

def run_program(program_id: str, env: Dict[str], args: List[str]) -> ExecResult:
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
        return ExecResult(exec_time, res.returncode, {}, res.stdout)


def run_program_times(program_id: str, env: Dict[str], args: List[str]):
    build_program(program_id)
    
        
