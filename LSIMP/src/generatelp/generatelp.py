"""
批量调用同目录下的 generate_lp 可执行文件。
命令格式: ./generate_lp <demand_file> <supply_file> <heat_file> <output_base>
示例: ./generate_lp demand.txt supply.txt heat.txt /path/to/output_base
输出文件一般生成在 output_base.lp。

无需命令行参数，可改参数集中在 __main__ 顶部。
"""

import logging
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import List, Tuple


def build_input_paths(date: str, demand_dir: Path, supply_dir: Path, heat_dir: Path) -> Tuple[Path, Path, Path]:
	demand = demand_dir / f"{date}.txt"
	supply = supply_dir / f"{date}.txt"
	heat = heat_dir / f"{date}.txt"
	return demand, supply, heat


def output_base_path(date: str, output_dir: Path) -> Path:
	return output_dir / date  # 传给可执行文件的 base（不带后缀）


def build_command(exe: Path, demand: Path, supply: Path, heat: Path, out_base: Path) -> List[str]:
	return [str(exe), str(demand), str(supply), str(heat), str(out_base)]


def validate_inputs(exe: Path, demand: Path, supply: Path, heat: Path) -> None:
	missing = [p for p in [exe, demand, supply, heat] if not p.exists()]
	if missing:
		for p in missing:
			logging.error("Missing required file: %s", p)
		raise FileNotFoundError(f"Missing inputs: {', '.join(str(p) for p in missing)}")
	if not exe.is_file():
		raise FileNotFoundError(f"Executable not found: {exe}")
	if not exe.stat().st_mode & 0o111:
		logging.warning("Executable may lack execute permission: %s", exe)


def run_single(date: str, exe: Path, demand_dir: Path, supply_dir: Path, heat_dir: Path, output_dir: Path) -> Tuple[str, int, Path]:
	demand, supply, heat = build_input_paths(date, demand_dir, supply_dir, heat_dir)
	out_base = output_base_path(date, output_dir)
	output_dir.mkdir(parents=True, exist_ok=True)

	validate_inputs(exe, demand, supply, heat)

	cmd = build_command(exe, demand, supply, heat, out_base)
	logging.info("Launching %s: %s", date, " ".join(cmd))

	result = subprocess.run(cmd, capture_output=True, text=True)
	if result.stdout:
		logging.debug("%s stdout: %s", date, result.stdout.strip())
	if result.stderr:
		logging.debug("%s stderr: %s", date, result.stderr.strip())

	# 可执行文件通常会写出 out_base.lp
	produced_lp = out_base.with_suffix(".lp")
	if result.returncode == 0 and not produced_lp.exists():
		logging.warning("Command succeeded but output not found: %s", produced_lp)

	return date, result.returncode, produced_lp


def run_batch(dates: List[str], exe: Path, demand_dir: Path, supply_dir: Path, heat_dir: Path, output_dir: Path, max_workers: int) -> None:
	logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")

	futures = []
	results = []
	with ThreadPoolExecutor(max_workers=max_workers) as executor:
		for date in dates:
			futures.append(
				executor.submit(run_single, date, exe, demand_dir, supply_dir, heat_dir, output_dir)
			)

		for fut in as_completed(futures):
			date, code, produced = fut.result()
			status = "OK" if code == 0 else f"EXIT {code}"
			logging.info("%s -> %s (lp: %s)", date, status, produced)
			results.append((date, code, produced))

	failed = [(d, c, p) for d, c, p in results if c != 0]
	if failed:
		logging.error("Failed %d/%d dates:", len(failed), len(results))
		for d, c, p in failed:
			logging.error("  - %s exited with %d (lp: %s)", d, c, p)
		raise SystemExit(1)
	else:
		logging.info("All dates finished successfully: %s", ", ".join(d for d, _, _ in results))


if __name__ == "__main__":
	# 集中可调参数
	DATA_ROOT = Path("/pub/netdisk1/lijy/AutoGD_exp_data/sample1000")
	DEMAND_DIR = DATA_ROOT / "demand/offline"
	SUPPLY_DIR = DATA_ROOT / "supply"
	HEAT_DIR = DATA_ROOT / "heat"
	OUTPUT_DIR = DATA_ROOT / "lp_generated"

	# 本目录下的可执行文件
	EXECUTABLE = Path(__file__).resolve().parent / "generate_lp"

	DATES = [
		"0531_01_01",
		"0531_01_02",
		"0531_01_03",
		"0531_01_04",
		"0531_01_05",
		"0531_01_06",
		"0531_01_07",
		"0531_01_08",
		"0531_01_09",
		"0531_01_10",
	]

	MAX_WORKERS = 4

	run_batch(
		dates=DATES,
		exe=EXECUTABLE,
		demand_dir=DEMAND_DIR,
		supply_dir=SUPPLY_DIR,
		heat_dir=HEAT_DIR,
		output_dir=OUTPUT_DIR,
		max_workers=MAX_WORKERS,
	)