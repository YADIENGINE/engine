# JsonIO

Source/API include root: `Engine/Core/Modules/JsonIO/`. Documentation and plans live here;
`Internals/` and `Tests/` paths below refer to the source module directory.

Stateless JSON parsing, serialization, file reading and atomic writing, with optional
authenticated encryption or write-only gzip compression. Include only `Modules/JsonIO/JsonIO.h`.

## Public API

| Operation | Input | Output / failure |
| --- | --- | --- |
| `Parse(text[, limits])` | UTF-8 JSON text | An owned `Document`; invalid input throws `Error`. |
| `Serialize(document[, options])` | An owned value tree | UTF-8 text; default format is Pretty. |
| `Read(path[, options])` | Explicit file path, limits, optional key | An owned `Document`; absent files throw MissingFile. |
| `Write(path, document[, options])` | Explicit path, value tree, limits, optional key or compression | Atomic replacement; previous destination survives failures before replacement. |

`Limits` defaults to 1 MiB and 128 nested containers. The byte limit applies to
input text, serialized text and the entire stored file, including encryption overhead
(for gzip: the compressed file).
Both limits must be positive. Documents/files are processed entirely in memory.
`SerializeOptions` and `WriteOptions` select Pretty (four spaces) or Compact.

`Document` is the module-owned public data contract, not a backend object. It holds
null, bool, signed/unsigned 64-bit integers, double, UTF-8 strings, `Object` or `Array`.
The root can have any of these types. `Object` is an ordered string map; serialized
object members consequently have deterministic key order. Source whitespace and
member order are not retained.

Document's converting constructors intentionally support object/array initializer
lists. They convert public values only and never invoke a codec or file operation.

- Construct objects/arrays with `Document::Object` / `Document::Array` or MakeObject/MakeArray.
- `operator[]` inserts a missing member of an existing object. It does not promote null/scalars.
- `At` reads an existing member without insertion; missing members throw MissingMember.
- `Append`, `Members` and `Elements` require the corresponding container type.
- `AsInt64` / `AsUInt64` allow lossless signed/unsigned conversion. An out-of-range
  integer throws NumberOutOfRange. Small positive parsed integers use signed storage.
- `AsDouble`, `AsString` and `AsBool` require their exact types. No implicit
  integer-to-floating conversion occurs. Equality treats equal signed/unsigned integers equally.
- Document copies own their complete trees. References into a tree follow standard
  map/vector invalidation rules. Document access never performs IO or serialization.

```cpp
using D = JsonIO::Document;
D document = D::Object{{"profile", D::Object{{"enabled", true}}}, {"items", D::Array{}}};
document["profile"]["enabled"] = false;
document["items"].Append("first");
JsonIO::Write("example.json", document);
auto loaded = JsonIO::Read("example.json");
const bool enabled = loaded.At("profile").At("enabled").AsBool();
auto compact = JsonIO::Serialize(loaded, {.format = JsonIO::Format::Compact});
```

### Encryption and errors

An empty options `key` explicitly selects plaintext. An encrypted operation borrows
exactly 32 key bytes for the duration of the call; the facade neither retains the
key nor creates/manages a key file. The caller owns and clears key memory.
Read with a key requires an encrypted envelope. Read without a key refuses an
encrypted envelope with EncryptionRequired; it never returns ciphertext as JSON.
Authentication failure never falls back to plaintext.

Write generates a fresh OS nonce, encrypts in memory, then writes only ciphertext
to its temporary file. The extension does not select encryption. See
[encrypted file format](Encryption.md).

### Gzip compression

`WriteOptions::compression = Compression::Gzip` stores the serialized text as one
standard RFC 1952 gzip member (readable by any gzip tool). It is write-only: `Read`
does not decompress, so reading such a file fails with InvalidJson. Combining gzip
with a key throws InvalidOptions before anything is written. Input beyond the
compressor's ~2 GiB limit throws LimitExceeded. The extension does not select compression.

All ordinary codec/file failures are reported as `JsonIO::Error`, carrying `Code()`
and a one-based `Byte()` when a parser position is available (zero otherwise).
Codes distinguish invalid JSON, type/member/range errors, limits, missing files,
IO/platform failures, encryption requirements, invalid envelopes/authentication,
and invalid options. Allocation failures are allowed to propagate.

Duplicate decoded object keys, invalid UTF-8, unpaired surrogate escapes, comments,
trailing commas, non-finite numbers and overflowing integer tokens are rejected.
No schema, save version, fallback document, backup or recovery policy is applied.

## Dependencies and replacement points

- `Internals/JsonCodec` and its private SAX reader adapt nlohmann/json 3.12.0.
  Replace these implementations and the private CMake dependency to swap JSON backends.
- `Internals/CryptoCodec` adapts Monocypher 4.0.3. Its primitive interface is separate
  from `EncryptedEnvelope`, which owns the existing file format. A backend swap must
  preserve algorithm, nonce/tag sizes and associated-data semantics for compatibility.
  Monocypher is vendored in the engine's `ThirdParty/Monocypher/` as its own C99 CMake target.
- `Internals/GzipCodec` adapts the vendored sdefl compressor (`Internals/sdefl.h`).
- Versions and licenses of all third-party code: [ThirdParty](ThirdParty.md).
- Engine-private `Platform/Internals/FileStorage`, `StorageError` and `SecureRandom` supply generic
  OS file/nonce primitives ([API boundaries](../../Contributing/ApiBoundaries.md)). They have no dependency on either module or on Game.
- The `EngineJsonIO` CMake target exposes no third-party headers or types publicly.

The facade has no instance, cache, registry or mutable global state. Independent
calls are reentrant. Callers must synchronize access to a shared mutable Document
and coordinate concurrent writes to the same file. Atomic replacement is not a
multi-file transaction or a multi-process read/modify/write lock.
