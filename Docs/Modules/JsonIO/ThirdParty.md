# Third-party code

All third-party code below is private to JsonIO; no type or header is exposed
through `JsonIO.h` or the `EngineJsonIO` CMake target.

This is the developer note. The notice text shipped with games lives in
[`ThirdParty/`](../../../ThirdParty/README.md); see [Release](../../Release.md).

- **nlohmann/json 3.12.0** ([release](https://github.com/nlohmann/json/releases/tag/v3.12.0)),
  MIT license, vendored unmodified in `Engine/ThirdParty/nlohmann_json/`. Its types are confined to
  `Internals/JsonCodec` and `Internals/JsonSax`.
- **Monocypher 4.0.3**, vendored unmodified in `Engine/ThirdParty/Monocypher/` with its
  shipped [license](../../../ThirdParty/Monocypher/LICENCE.md) and
  [README](../../../ThirdParty/Monocypher/README.md). Built as the separate C99 `Monocypher` target and
  used only by `Internals/CryptoCodec`.
- **sdefl** (Small Deflate by Micha Mettke), `Internals/sdefl.h`, copied without
  modifications from [raylib 5.5](https://github.com/raysan5/raylib/blob/5.5/src/external/sdefl.h).
  The original MIT/Public Domain license notice is retained in the header.
  `Internals/GzipCodec.cpp` compiles this copy with prefixed exported names to
  coexist with raylib's copy; no raylib headers or runtime are needed. The gzip
  header/CRC32/trailer follow [RFC 1952](https://www.rfc-editor.org/rfc/rfc1952).

`Engine/Core/Tests/Support/third_party/sinfl.h` is the matching inflater from
[raylib 5.5](https://github.com/raysan5/raylib/blob/5.5/src/external/sinfl.h), used
only by tests (`Engine/Core/Tests/Support/GzipInflate`). It retains the same original license
notices and is not linked into the game. Its MSVC `_BitScanReverse` branch uses an
`unsigned long` result instead of passing `unsigned int*`, so the header also compiles as C++.
