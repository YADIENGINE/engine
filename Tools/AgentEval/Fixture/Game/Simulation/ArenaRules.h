#pragma once

// The arena's numbers. Distances are canvas units (1280 x 720), speeds are units per second.
inline constexpr float ArenaLeft = 40.0f;
inline constexpr float ArenaTop = 40.0f;
inline constexpr float ArenaRight = 1240.0f;
inline constexpr float ArenaBottom = 680.0f;

inline constexpr float PlayerSpeed = 300.0f;
inline constexpr float PlayerRadius = 16.0f;
inline constexpr int StartLives = 3;
inline constexpr int InvulnerableSteps = 90; // After a hit

inline constexpr float EnemySpeed = 120.0f;
inline constexpr float EnemyRadius = 14.0f;
inline constexpr int SpawnEverySteps = 180;
inline constexpr int MaxEnemies = 6;
inline constexpr float SpawnMinDistance = 300.0f; // From the player

inline constexpr float CoinRadius = 10.0f;
inline constexpr int CoinScore = 1;
