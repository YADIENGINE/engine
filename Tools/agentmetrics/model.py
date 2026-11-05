"""What one agent session cost: its model requests and tool calls, the same shape for every harness."""

from collections.abc import Iterator
from dataclasses import dataclass, field
from pathlib import Path

CHARS_PER_TOKEN = 4  # Tool output is measured in characters; tokens are estimated with this ratio


@dataclass
class Request:
    """One model request. `context` is everything the model read for it; `output` includes reasoning."""

    input: int = 0  # Uncached input
    cache_read: int = 0
    cache_write: int = 0
    output: int = 0
    reasoning: int = 0  # Part of output, when the harness reports it
    model: str = ""
    effort: str = ""  # Reasoning effort the harness logged for the request

    @property
    def context(self) -> int:
        return self.input + self.cache_read + self.cache_write

    @property
    def total(self) -> int:
        return self.context + self.output


@dataclass
class ToolCall:
    """One tool call. `category` groups calls (read, search, edit, engine, shell, ...); `target` is the file or the
    command; `engine` names the first engine command it ran, such as "build" or "ctl status", and `engines` all of
    them: a `;` chain or a script over a list of queries runs several in one call."""

    name: str
    category: str
    target: str = ""
    chars: int = 0  # Characters of output the agent got back
    tokens: int | None = None  # Exact output tokens when the harness reports them
    images: int = 0
    failed: bool = False
    engine: str | None = None
    files: list[str] = field(default_factory=list)  # Files the call read
    roots: list[str] = field(default_factory=list)  # Folders a search covered; "." is where the agent runs
    engines: list[str] = field(default_factory=list)

    @property
    def output_tokens(self) -> int:
        return self.tokens if self.tokens is not None else self.chars // CHARS_PER_TOKEN

    def engine_runs(self) -> list[str]:
        """Every engine command the call ran, in order."""
        return self.engines or ([self.engine] if self.engine else [])


@dataclass
class Session:
    harness: str  # "claude" or "codex"
    id: str
    path: Path
    cwd: str = ""
    started: str = ""  # ISO timestamps
    ended: str = ""
    title: str = ""
    parent: str | None = None  # Set for subagent sessions
    requests: list[Request] = field(default_factory=list)
    tools: list[ToolCall] = field(default_factory=list)
    prompts: int = 0  # Messages the user typed
    compactions: int = 0

    @property
    def subagent(self) -> bool:
        return self.parent is not None

    @property
    def models(self) -> list[str]:
        return sorted({request.model for request in self.requests if request.model})

    @property
    def efforts(self) -> list[str]:
        return sorted({request.effort for request in self.requests if request.effort})

    def sum(self, name: str) -> int:
        return sum(getattr(request, name) for request in self.requests)

    @property
    def peak_context(self) -> int:
        return max((request.context for request in self.requests), default=0)

    @property
    def startup_context(self) -> int:
        """Context of the first request: system prompt, tools and always-loaded agent files."""
        return self.requests[0].context if self.requests else 0


def lines(path: Path) -> Iterator[str]:
    """A log's lines, read as they come; the file closes when the reading ends."""
    with path.open(encoding="utf-8", errors="replace") as file:
        yield from file
