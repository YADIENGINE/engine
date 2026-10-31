# Module Rules

## What is a module

The baseline is the game developer. The engine core holds what every game touches:
the loop and scenes, window and canvas, rendering, input, time, and the player
options every game is expected to have (`Options/GameOptions`). A module is a system
that a developer whose game does not need it never has to think about: it has no
field they must fill, nothing they must call, and it does no work on its own.
A game opts into a module by creating and using it; a module may depend on the
engine core and on modules it declares.

Known exception: the core uses `Persistence` (and through it `JsonIO`) to store its
options. It stays a module because the developer still meets it only when the game
has saves of its own; the core never exposes it for its own options.

The structure below (one facade, `Internals/`, central documentation, tests) is a quality standard;
it does not by itself make something a module.

Every module in `Engine/Core/Modules/` must follow these rules. They define a consistent
structure, a clear public API, and strict implementation boundaries. Keep these
rules in mind when creating, extending, or using a module.

## 1. Directory structure

Each module must have its own directory, named after the module:

```text
ModuleName/
    Internals/
    ModuleName.h
    ModuleName.cpp
    Tests/
        CMakeLists.txt
        *Tests.cpp
```

- `Internals/` contains private implementation files that consumers must not use directly.
- `Docs/Modules/ModuleName/README.md` starts with a short module summary,
  followed by public API documentation, notes and examples.
- `ModuleName.h` declares the module's main class and its public API.
- `ModuleName.cpp` implements the main class.
- Documentation lives outside the module's source tree in the engine's `Docs/Modules/ModuleName/`.
  Keep supported API in `README.md`, planned work in `Roadmap.md`, and additional
  format/reference guides beside them. Third-party sources live unmodified in the engine's `ThirdParty/` with their
  licenses; code that ends up in a game is also listed in `ThirdParty/Notices.json` ([ThirdParty](../../ThirdParty/README.md)).
- `Tests/` holds the module's Catch2 tests. Its `CMakeLists.txt` is one
  `engine_tests(Engine<ModuleName>Tests SOURCES ... LINKS Engine<ModuleName> ...)` call,
  discovered automatically when tests are enabled; it is never part of a production target.
  Tests use only the public header and link only this module and its declared
  dependencies (plus `EngineTestSupport`). Game code never tests module behavior.
  See `Docs/Contributing/Testing.md`.

The corresponding documentation tree is:

```text
Docs/Modules/ModuleName/
    README.md
    Roadmap.md
```

## 2. One public entry point

All supported API operations must be declared in `ModuleName.h`. The main
class's `public` members are the supported interface; its `private` members are
implementation details that consumers do not need to know or access.

Consumers must interact with the module only through this class. Do not expose
alternative entry points through internal classes, helper functions, or global
state. Do not return internal implementation objects that let consumers bypass
the main class.

Public value contracts (documents, options, results and errors) may be declared in
the main header and used directly. Their value accessors are not alternative
service entry points: they must not perform IO, invoke a backend, or expose an
internal implementation object. This is how JsonIO's owned Document is used.

## 3. Independence and dependency boundaries

Modules must be as independent as possible. A small number of shared dependencies
is acceptable; for example, other modules may use Persistence through its public
API. Every such dependency must be justified and listed in the module's README.

A module must never include or access another module's `Internals/` files or
private implementation details ([API boundaries](ApiBoundaries.md); checked by `Tools/include_boundaries.py`). Circular module dependencies are not allowed.
Engine modules must remain independent of `Game/` code and game-specific rules.

## 4. Focused responsibilities

Each module must have one clear responsibility. Keep its public API small and
purposeful; implementation convenience alone is not a reason to add a public
member. Module-specific helpers belong inside the module, not in unrelated
modules or shared utility collections.

## 5. Explicit ownership and lifecycle

Document who owns resources and how long borrowed references remain valid.
Initialization, shutdown, and any required call order must be clear from the API
documentation. Manage owned resources with RAII and avoid hidden mutable global
state.

## 6. A minimal, self-contained public header

`ModuleName.h` must be usable without requiring consumers to include other
headers first. Keep implementation-only includes in `.cpp` files or `Internals/`
and prefer forward declarations where practical. Internal types must not become
part of the public contract.

## 7. Documentation stays current

Update the module's README whenever its public API or observable behavior changes.
Describe parameters, return values, failure behavior, and relevant constraints
such as thread safety where applicable. Examples must use only the public API.
Keep planned work in `Docs/Modules/ModuleName/Roadmap.md`; a TODO is not a supported feature.
