"""The agent's scenario must notice when the grace period after a hit is gone: with it removed, the scenario fails."""

import subprocess
import sys
from pathlib import Path

project = Path(sys.argv[1])
rules = project / "Game/Simulation/ArenaRules.h"
original = rules.read_text(encoding="utf-8")
mutated = original.replace("InvulnerableSteps = 90;", "InvulnerableSteps = 0;")
if mutated == original:
    sys.exit("ArenaRules.h no longer holds 'InvulnerableSteps = 90;'; the check cannot remove the grace period")

scenarios = [path for path in sorted((project / "Game/Simulation/Scenarios").glob("*.json"))
             if path.name != "Smoke.json" and "PlayerHit" in path.read_text(encoding="utf-8")]
if not scenarios:
    sys.exit("no new scenario mentions PlayerHit")
rules.write_text(mutated, encoding="utf-8", newline="\n")
try:
    caught = []
    for scenario in scenarios:
        done = subprocess.run([sys.executable, "Engine", "sim", "run", scenario.relative_to(project).as_posix()],
                              cwd=project, capture_output=True, text=True)
        print(f"{scenario.name}: exit {done.returncode} without the grace period")
        caught.append(done.returncode == 1)
finally:
    rules.write_text(original, encoding="utf-8", newline="\n")
sys.exit(0 if any(caught) else "no scenario fails when the grace period is removed")
