@Engine/AgentDocs/Agent.md

Arena: a small top-down game. The player collects coins while enemies chase them.

- The rules are a simulation in `Game/Simulation` (id `arena.play`); the scene that draws them is `Game/Scenes/PlayScene`.
- Numbers live in `Game/Simulation/ArenaRules.h`.
- Scenarios in `Game/Simulation/Scenarios` run with `python Engine sim run FILE`.
