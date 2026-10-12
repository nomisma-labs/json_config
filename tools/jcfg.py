# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary
"""Resolve jcfg sections to application JSON without writing output files."""

from collections.abc import Iterable, Mapping
from copy import deepcopy
from dataclasses import dataclass
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
from typing import Any


__all__ = ["config_error", "resolved_config", "generate_configs", "generate_configs_with_inputs"]


class config_error(ValueError):
    """Invalid source metadata, application instructions, or imported JSON."""


@dataclass(frozen=True)
class resolved_config:
    """A selected section's output filename and resolved application object."""

    name: str
    gen_file: str
    config: dict[str, Any]


@dataclass(frozen=True)
class _location:
    source: Path
    section: str | None = None
    pointer: str = ""

    def child(self, key: str | int) -> "_location":
        token = str(key).replace("~", "~0").replace("/", "~1")
        return _location(self.source, self.section, f"{self.pointer}/{token}")

    def error(self, message: str) -> config_error:
        section = f" [section {self.section!r}]" if self.section is not None else ""
        return config_error(f"{self.source}{section} at {self.pointer or '<root>'}: {message}")


@dataclass
class _section:
    name: str
    parent: str | None
    gen_file: str
    generate: bool
    imports: list[str]
    data: dict[str, Any]


def _parse_finite_float(text: str) -> float:
    value = float(text)
    if not math.isfinite(value):
        raise ValueError(f"number is outside the supported finite range: {text}")
    return value


def _reject_json_constant(text: str) -> None:
    raise ValueError(f"invalid JSON constant: {text}")


def _read_json(path: Path, location: _location) -> Any:
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeError, ValueError) as error:
        raise location.error(f"cannot read {path}: {error}") from error
    try:
        return json.loads(text, parse_constant=_reject_json_constant, parse_float=_parse_finite_float)
    except ValueError as error:
        raise location.error(f"invalid JSON in {path}: {error}") from error


def _validate_literal_string(value: Any, location: _location) -> str:
    if not isinstance(value, str) or not value:
        raise location.error("expected a nonempty string")
    if "jcfg:" in value:
        raise location.error("metadata must be literal; placeholders are not allowed")
    return value


def _validate_output_name(value: Any, location: _location) -> str:
    name = _validate_literal_string(value, location)
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or not path.name or "\0" in name:
        raise location.error("output name must be relative and cannot contain path traversal")
    return name


def _read_sections(source: Path) -> list[_section]:
    location = _location(source)
    document = _read_json(source, location)
    entries = [document] if isinstance(document, dict) else document
    if not isinstance(entries, list) or not entries:
        raise location.error("expected a section object or a nonempty array of sections")

    sections: dict[str, _section] = {}
    default_filename = (source.name[:-len("_jcfg.json")] + ".json"
                        if source.name.endswith("_jcfg.json") else source.name)
    for index, entry in enumerate(entries):
        entry_location = location.child(index) if isinstance(document, list) else location
        if not isinstance(entry, dict) or not isinstance(entry.get("jcfg"), dict):
            raise entry_location.error("each section requires a jcfg metadata object")
        metadata = entry["jcfg"]
        name = _validate_output_name(metadata.get("name"), entry_location.child("jcfg").child("name"))
        current = _location(source, name).child("jcfg")
        if name in sections:
            raise current.child("name").error(f"duplicate section {name!r}")
        for key in metadata:
            if key not in {"name", "extend", "gen_file", "generate", "import"}:
                raise current.child(key).error(f"unknown metadata property {key!r}")

        parent = None
        if not sections:
            if "extend" in metadata:
                raise current.child("extend").error("the first section cannot extend another section")
        else:
            parent = _validate_literal_string(metadata.get("extend"), current.child("extend"))
            if parent not in sections:
                raise current.child("extend").error(f"parent {parent!r} must name an earlier section")

        filename = metadata.get("gen_file", sections[parent].gen_file if parent else default_filename)
        filename = _validate_output_name(filename, current.child("gen_file"))
        generate = metadata.get("generate", True)
        if not isinstance(generate, bool):
            raise current.child("generate").error("generate must be a boolean")
        imports = metadata.get("import", [])
        if isinstance(imports, str):
            imports = [imports]
        if not isinstance(imports, list) or any(not isinstance(item, str) or not item for item in imports):
            raise current.child("import").error("import must be a filename or an array of filenames")
        data = {key: value for key, value in entry.items() if key != "jcfg"}
        sections[name] = _section(name, parent, filename, generate, imports, data)
    return list(sections.values())


def _merge_literal_json(previous: Any, incoming: Any) -> Any:
    """Merge literal JSON; this function never interprets instructions."""
    if isinstance(previous, dict) and isinstance(incoming, dict):
        merged = deepcopy(previous)
        for key, value in incoming.items():
            merged[key] = _merge_literal_json(merged.get(key), value)
        return merged
    return deepcopy(incoming)


def _select_json_fragment(document: Any, pointer: str, reference: str, location: _location) -> Any:
    if not pointer:
        return document
    if not pointer.startswith("/"):
        raise location.error(f"invalid JSON Pointer in {reference!r}: expected a leading '/'")
    value = document
    for encoded in pointer[1:].split("/"):
        if re.search(r"~(?![01])", encoded):
            raise location.error(f"invalid JSON Pointer escape in {reference!r}")
        key = encoded.replace("~1", "/").replace("~0", "~")
        if isinstance(value, dict) and key in value:
            value = value[key]
        elif isinstance(value, list) and re.fullmatch(r"0|[1-9][0-9]*", key):
            # Compare lengths first so an arbitrarily long index is still a clean error.
            limit = str(len(value))
            if len(key) > len(limit) or (len(key) == len(limit) and key >= limit):
                raise location.error(f"array index out of range in {reference!r}")
            value = value[int(key)]
        else:
            raise location.error(f"missing fragment or invalid array index in {reference!r}")
    return value


class _resolver:
    def __init__(self, source: Path, environment: Mapping[str, str]):
        self.source = source
        self.environment = environment
        self.donors: dict[Path, Any] = {}

    def expand_generation_variables(self, text: str, location: _location,
                                    *, allow_runtime: bool = True) -> str:
        # Scan authored text once. Environment values are data, not more instructions.
        parts = []
        end = 0
        for match in re.finditer(r"\{jcfg:(gen-var|run-var)\s+([^{}\s]+)\}", text):
            literal = text[end:match.start()]
            if "jcfg:" in literal:
                raise location.error("unknown or malformed instruction or placeholder")
            parts.append(literal)
            kind, name = match.groups()
            if kind == "run-var":
                if not allow_runtime:
                    raise location.error("runtime placeholders cannot be used in import paths")
                parts.append(match.group())
            else:
                if name not in self.environment:
                    raise location.error(f"unset generation variable {name!r}")
                value = self.environment[name]
                if not isinstance(value, str):
                    raise location.error(f"generation variable {name!r} must be a string")
                parts.append(value)
            end = match.end()
        if "jcfg:" in text[end:]:
            raise location.error("unknown or malformed instruction or placeholder")
        parts.append(text[end:])
        return "".join(parts)

    def import_value(self, text: str, location: _location, *, whole_object: bool = False) -> Any:
        reference = self.expand_generation_variables(text, location, allow_runtime=False)
        filename, separator, pointer = reference.partition(":")
        if not filename or "\0" in filename:
            raise location.error("import requires a nonempty filename without null characters")
        if whole_object and separator:
            raise location.error("section imports require whole files; ':' is reserved in donor paths")
        path = Path(filename)
        if not path.is_absolute():
            path = self.source.parent / path
        if path not in self.donors:
            self.donors[path] = _read_json(path, location)
        value = _select_json_fragment(self.donors[path], pointer, reference, location)
        if whole_object and not isinstance(value, dict):
            raise location.error(f"section import {reference!r} must contain a JSON object")
        return value

    def merge_jcfg_values(self, previous: Any, authored: Any, location: _location) -> Any:
        if isinstance(authored, dict):
            if "jcfg:override" in authored:
                if len(authored) != 1:
                    raise location.error("an override wrapper must contain only 'jcfg:override'")
                return self.merge_jcfg_values(None, authored["jcfg:override"],
                                              location.child("jcfg:override"))
            result = deepcopy(previous) if isinstance(previous, dict) else {}
            for key, value in authored.items():
                child = location.child(key)
                if "jcfg:" in key:
                    raise child.error("unknown instruction in property name")
                if value == "jcfg:delete":
                    if key not in result:
                        raise child.error("cannot delete a missing property")
                    del result[key]
                else:
                    result[key] = self.merge_jcfg_values(result.get(key), value, child)
            return result
        if isinstance(authored, list):
            return [self.merge_jcfg_values(None, value, location.child(index))
                    for index, value in enumerate(authored)]
        if isinstance(authored, str):
            command, _, argument = authored.partition(" ")
            if command == "jcfg:import":
                return _merge_literal_json(previous, self.import_value(argument.strip(), location))
            if authored == "jcfg:delete":
                raise location.error("delete requires an existing object property")
            return self.expand_generation_variables(authored, location)
        return authored


def generate_configs(source: str | Path, *, sections: Iterable[str] | None = None,
                     environment: Mapping[str, str] | None = None) -> dict[str, resolved_config]:
    """Return resolved configs keyed by section name, in source order.

    By default, select every section whose generate flag is true. Explicitly
    selecting a suppressed or unknown section fails. All metadata is checked,
    but only selected sections and their ancestors require external inputs.
    An explicit environment replaces the process environment for this call.
    No files are written.
    """
    configs, _ = generate_configs_with_inputs(source, sections=sections, environment=environment)
    return configs


def generate_configs_with_inputs(source: str | Path, *, sections: Iterable[str] | None = None,
                                 environment: Mapping[str, str] | None = None
                                 ) -> tuple[dict[str, resolved_config], set[Path]]:
    """Return (configs, input_paths), using the same options as generate_configs.

    Configs are keyed by section name, in source order. Input paths are a set of
    absolute Path objects with symbolic links resolved, including the source and
    donors read for selected sections and their ancestors. No files are written.
    """
    source = Path(source).absolute()
    definitions = _read_sections(source)
    by_name = {section.name: section for section in definitions}
    selected = ({section.name for section in definitions if section.generate}
                if sections is None else set([sections] if isinstance(sections, str) else sections))
    for name in sorted(selected):
        location = _location(source, name)
        if name not in by_name:
            raise location.error("unknown selected section")
        if not by_name[name].generate:
            raise location.error("cannot select a section with generate set to false")

    needed = set(selected)
    for section in reversed(definitions):
        if section.name in needed and section.parent is not None:
            needed.add(section.parent)

    resolver = _resolver(source, dict(os.environ if environment is None else environment))
    resolved: dict[str, dict[str, Any]] = {}
    outputs: dict[str, resolved_config] = {}
    for section in definitions:
        if section.name not in needed:
            continue
        location = _location(source, section.name)
        data = resolved[section.parent] if section.parent is not None else {}
        for index, reference in enumerate(section.imports):
            donor = resolver.import_value(reference, location.child("jcfg").child("import").child(index),
                                          whole_object=True)
            data = _merge_literal_json(data, donor)
        data = resolver.merge_jcfg_values(data, section.data, location)
        if not isinstance(data, dict):
            raise location.error("resolved application config must be a JSON object")
        resolved[section.name] = data
        if section.name in selected:
            outputs[section.name] = resolved_config(section.name, section.gen_file, deepcopy(data))
    inputs = {source.resolve(), *(path.resolve() for path in resolver.donors)}
    return outputs, inputs
