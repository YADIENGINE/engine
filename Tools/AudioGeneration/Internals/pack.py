"""Named sound sets: recipe registration, per-sound seeds, finishing, command line and summaries."""

import argparse
import inspect
import itertools
from pathlib import Path

FINISH = {"peak": 0.9, "lufs": None, "ceiling": 0.98, "dc": True, "fade_in": 0.0005, "fade_out": 0.01,
          "trim": -60.0}
SOUND = {"output": None, "variation": 0}
FORMATS = ("wav", "ogg", "flac")


def _options(defaults, overrides):
    unknown = set(overrides) - set(FINISH) - set(SOUND)
    if unknown:
        raise ValueError(f"Unknown sound option(s): {', '.join(sorted(unknown))}")
    output = overrides.get("output")
    if output is not None and Path(output).is_absolute():
        raise ValueError("A sound's output must be a folder relative to the pack output")
    options = {**SOUND, **defaults, **overrides}
    # Choosing one level target per sound replaces the other default.
    if "lufs" in overrides and "peak" not in overrides:
        options["peak"] = None
    if "peak" in overrides and "lufs" not in overrides:
        options["lufs"] = None
    return options


def _value(text):
    try:
        return int(text)
    except ValueError:
        try:
            return float(text)
        except ValueError:
            return text


def _assignments(items):
    """--set name=v1,v2 → {name: [v1, v2]}."""
    values = {}
    for item in items:
        name, separator, listed = item.partition("=")
        if not separator or not name.isidentifier() or not listed:
            raise ValueError(f"--set expects NAME=VALUE[,VALUE...], got {item!r}")
        values[name] = [_value(text) for text in listed.split(",")]
    return values


class SoundPack:
    """Created by SoundGenerator.pack(); register recipes with @pack.sound(name, **options)."""

    def __init__(self, generator, output, prefix, audio_format, bits, dither, defaults):
        if not isinstance(prefix, str):
            raise ValueError("prefix must be a string")
        if audio_format not in FORMATS:
            raise ValueError(f"format must be one of: {', '.join(FORMATS)}")
        if "output" in defaults or "variation" in defaults:
            raise ValueError("output and variation are per-sound options")
        self.generator = generator
        self.output = Path(output)
        self.prefix = prefix
        self.format = audio_format
        self._save = {"bits": bits, "dither": dither}
        self._defaults = _options(FINISH, defaults)
        self._sounds = {}

    @property
    def names(self):
        return list(self._sounds)

    def sound(self, name, **options):
        """Decorator: recipe(g, **parameters) receives the sound's seeded context and returns mono samples.

        Keyword parameters with defaults can be explored from the command line with --set.
        """
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

    def parameters(self, name):
        """Keyword parameters the recipe accepts, with their defaults."""
        signature = inspect.signature(self._entry(name)[0])
        return {key: parameter.default for key, parameter in list(signature.parameters.items())[1:]
                if parameter.default is not inspect.Parameter.empty}

    def render(self, name, *, variation=None, parameters=None):
        """Run the recipe in its named context and apply the finishing chain."""
        recipe, options = self._entry(name)
        parameters = dict(parameters or {})
        unknown = set(parameters) - set(self.parameters(name))
        if unknown:
            raise ValueError(f"{name} has no parameter(s) {', '.join(sorted(unknown))}")
        context = self.generator.for_sound(name, variation=options["variation"] if variation is None else variation)
        samples = recipe(context, **parameters)
        return context.finish(samples, peak=options["peak"], lufs=options["lufs"], ceiling=options["ceiling"],
                              dc=options["dc"], fade_in=options["fade_in"], fade_out=options["fade_out"],
                              trim=options["trim"])

    def path(self, name, output=None):
        target = self._entry(name)[1]["output"]
        root = Path(output) if output is not None else self.output
        folder = root if target is None else root / target
        return folder / f"{self.prefix}{name}.{self.format}"

    def write(self, name, output=None):
        samples = self.render(name)
        return self.generator.save(self.path(name, output), samples, **self._save), samples

    def _takes(self, name, variations, assignments):
        """(variation, parameters) for every combination the command line asks for."""
        accepted = self.parameters(name)
        grid = {key: values for key, values in assignments.items() if key in accepted}
        base = self._entry(name)[1]["variation"]
        numbers = [base] if variations is None else range(variations)
        combos = [dict(zip(grid, values)) for values in itertools.product(*grid.values())] if grid else [{}]
        return [(number, combo) for number in numbers for combo in combos]

    def main(self, argv=None):
        parser = argparse.ArgumentParser(description=f"Render the {self.prefix or 'sound'} set.")
        parser.add_argument("names", nargs="*", metavar="NAME", help="sounds to render (default: all)")
        parser.add_argument("--output", type=Path, help=f"output folder instead of {self.output}")
        parser.add_argument("--check", action="store_true", help="render and measure without writing files")
        parser.add_argument("--gallery", type=Path, metavar="HTML", help="also write a listening page")
        parser.add_argument("--compare", action="store_true",
                            help="compare with the files on disk instead of writing (implies --check)")
        parser.add_argument("--variations", type=int, metavar="N",
                            help="render seed variations 0..N-1 of each sound (implies --check)")
        parser.add_argument("--set", action="append", default=[], metavar="NAME=V1,V2",
                            help="render each value of a recipe parameter (implies --check; repeatable)")
        parser.add_argument("--list", action="store_true", help="print the sounds and their parameters, then exit")
        args = parser.parse_args(argv)
        if args.list:
            for name in self._sounds:
                parameters = self.parameters(name)
                print(name + ("  " + ", ".join(f"{key}={value}" for key, value in parameters.items())
                              if parameters else ""))
            return {}
        unknown = [name for name in args.names if name not in self._sounds]
        if unknown:
            parser.error(f"unknown sound(s): {', '.join(unknown)}. Choose: {', '.join(self._sounds)}")
        if args.variations is not None and args.variations < 1:
            parser.error("--variations must be at least 1")
        try:
            assignments = _assignments(args.set)
        except ValueError as error:
            parser.error(str(error))
        names = args.names or list(self._sounds)
        for key in assignments:
            if not any(key in self.parameters(name) for name in names):
                parser.error(f"no selected sound has a parameter named {key!r}")
        exploring = args.variations is not None or bool(assignments)
        writing = not (args.check or args.compare or exploring)
        if not writing and not args.check:
            print("Exploring/comparing: no files are written.")
        rendered, entries = {}, []
        for name in names:
            path = self.path(name, args.output)
            current = None
            if args.compare:
                current = self.generator.load(path) if path.exists() else None
            for variation, parameters in self._takes(name, args.variations, assignments):
                label = path.name
                if exploring:
                    label += f"  v{variation}" + "".join(f"  {key}={value}" for key, value in parameters.items())
                if writing:
                    path, samples = self.write(name, args.output)
                else:
                    samples = self.render(name, variation=variation, parameters=parameters)
                rendered[label] = samples
                print(summary(self.generator, label, samples))
                if args.compare:
                    print("    current: " + (summary(self.generator, "", current).strip()
                                             if current is not None else "no file on disk"))
                entries.append({"name": label, "group": name, "samples": samples, "current": current,
                                "choice": {"sound": name, "variation": variation, "parameters": parameters}})
        if args.gallery:
            from . import gallery

            print(gallery.export(self.generator, args.gallery, entries, self.prefix.rstrip("_") or "Sounds"))
        return rendered


def summary(generator, label, samples):
    report = generator.analyze(samples)

    def level(value, unit):
        return f"{value:6.1f} {unit}" if value is not None else f"{'n/a':>6} {unit}"

    warnings = " ".join(f"[{warning['code']}]" for warning in report["warnings"])
    return (f"{label:32s} {report['duration_seconds']:5.2f} s  peak {level(report['peak_dbfs'], 'dBFS')}  "
            f"{level(report['loudness_lufs'], 'LUFS')}  {warnings}").rstrip()
