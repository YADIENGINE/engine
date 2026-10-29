"""Named sound sets: recipe registration, per-sound seeds, finishing, command line and summaries."""

import argparse
from pathlib import Path

FINISH = {"peak": 0.9, "lufs": None, "ceiling": 0.98, "dc": True, "fade_in": 0.0005, "fade_out": 0.01,
          "trim": -60.0}


def _options(defaults, overrides):
    unknown = set(overrides) - set(FINISH) - {"output"}
    if unknown:
        raise ValueError(f"Unknown sound option(s): {', '.join(sorted(unknown))}")
    output = overrides.get("output")
    if output is not None and Path(output).is_absolute():
        raise ValueError("A sound's output must be a folder relative to the pack output")
    options = {"output": None, **defaults, **overrides}
    # Choosing one level target per sound replaces the other default.
    if "lufs" in overrides and "peak" not in overrides:
        options["peak"] = None
    if "peak" in overrides and "lufs" not in overrides:
        options["lufs"] = None
    return options


class SoundPack:
    """Created by SoundGenerator.pack(); register recipes with @pack.sound(name, **options)."""

    def __init__(self, generator, output, prefix, defaults):
        if not isinstance(prefix, str):
            raise ValueError("prefix must be a string")
        self.generator = generator
        self.output = Path(output)
        self.prefix = prefix
        self._defaults = _options(FINISH, defaults)
        self._sounds = {}

    @property
    def names(self):
        return list(self._sounds)

    def sound(self, name, **options):
        """Decorator: recipe(g) receives the sound's own seeded context and returns mono samples."""
        if not isinstance(name, str) or not name or name in self._sounds:
            raise ValueError(f"Sound names must be unique nonempty strings: {name!r}")
        options = _options(self._defaults, options)

        def register(recipe):
            self._sounds[name] = (recipe, options)
            return recipe

        return register

    def _entry(self, name):
        if name not in self._sounds:
            raise KeyError(f"Unknown sound {name!r}. Choose: {', '.join(self._sounds)}")
        return self._sounds[name]

    def render(self, name):
        """Run the recipe in its named context and apply the finishing chain."""
        recipe, options = self._entry(name)
        context = self.generator.for_sound(name)
        samples = recipe(context)
        return context.finish(samples, peak=options["peak"], lufs=options["lufs"], ceiling=options["ceiling"],
                              dc=options["dc"], fade_in=options["fade_in"], fade_out=options["fade_out"],
                              trim=options["trim"])

    def path(self, name, output=None):
        target = self._entry(name)[1]["output"]
        root = Path(output) if output is not None else self.output
        folder = root if target is None else root / target
        return folder / f"{self.prefix}{name}.wav"

    def write(self, name, output=None):
        samples = self.render(name)
        return self.generator.write_wav(self.path(name, output), samples), samples

    def main(self, argv=None):
        parser = argparse.ArgumentParser(description=f"Render the {self.prefix or 'sound'} set to WAV files.")
        parser.add_argument("names", nargs="*", metavar="NAME", help="sounds to render (default: all)")
        parser.add_argument("--output", type=Path, help=f"output folder instead of {self.output}")
        parser.add_argument("--check", action="store_true", help="render and measure without writing files")
        parser.add_argument("--gallery", type=Path, metavar="HTML", help="also write a listening page")
        parser.add_argument("--list", action="store_true", help="print the sound names and exit")
        args = parser.parse_args(argv)
        if args.list:
            print("\n".join(self._sounds))
            return {}
        unknown = [name for name in args.names if name not in self._sounds]
        if unknown:
            parser.error(f"unknown sound(s): {', '.join(unknown)}. Choose: {', '.join(self._sounds)}")
        rendered = {}
        for name in args.names or self._sounds:
            if args.check:
                path, samples = self.path(name, args.output), self.render(name)
            else:
                path, samples = self.write(name, args.output)
            rendered[path.name] = samples
            print(summary(self.generator, path.name, samples))
        if args.gallery:
            print(self.generator.export_gallery(args.gallery, rendered, title=self.prefix.rstrip("_") or "Sounds"))
        return rendered


def summary(generator, label, samples):
    report = generator.analyze(samples)

    def level(value, unit):
        return f"{value:6.1f} {unit}" if value is not None else f"{'n/a':>6} {unit}"

    warnings = " ".join(f"[{warning['code']}]" for warning in report["warnings"])
    return (f"{label:32s} {report['duration_seconds']:5.2f} s  peak {level(report['peak_dbfs'], 'dBFS')}  "
            f"{level(report['loudness_lufs'], 'LUFS')}  {warnings}").rstrip()
