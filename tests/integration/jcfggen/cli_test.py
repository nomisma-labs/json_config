# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary
"""Exercise the generator's public command line from an unrelated directory."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


repository = Path(__file__).resolve().parents[3]
generator = repository / "tools/jcfggen.py"
examples = repository / "docs/json_config/examples"


class cli_tests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.directory = Path(directory.name)
        self.inputs = self.directory / "config"
        self.outputs = self.directory / "genconfig"
        self.working = self.directory / "unrelated"
        self.working.mkdir()
        self.environment = dict(os.environ)
        self.environment.pop("JCFG_TEST_MISSING_VALUE", None)

    def write_json(self, path, content):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(content), encoding="utf-8")
        return path

    def source(self, content, app="app"):
        return self.write_json(self.inputs / app / f"{app}_jcfg.json", content)

    def run_cli(self, *arguments, success=True):
        result = subprocess.run(
            [sys.executable, str(generator), *map(str, arguments)],
            cwd=self.working, env=self.environment,
            capture_output=True, text=True, check=False, timeout=20,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertTrue(result.stderr.strip(), "failure should explain its cause")
            self.assertNotIn("Traceback (most recent call last)", result.stderr)
        return result

    def generate(self, source, *arguments, success=True):
        return self.run_cli("--source", source, "--output-dir", self.outputs,
                            *arguments, success=success)

    def files(self):
        return {path.relative_to(self.outputs).as_posix(): path.read_bytes()
                for path in self.outputs.rglob("*") if path.is_file()}

    def read_output(self, name):
        return json.loads((self.outputs / name).read_text(encoding="utf-8"))

    def test_direct_source_from_unrelated_working_directory(self):
        source = self.source({"jcfg": {"name": "dev"}, "enabled": True})
        self.generate(source)
        self.assertEqual(set(self.files()), {"dev/app.json"})
        self.assertEqual(self.read_output("dev/app.json"), {"enabled": True})

    def test_relative_command_line_paths_follow_working_directory(self):
        self.source({"jcfg": {"name": "dev"}, "value": 7})
        self.run_cli("--source", "../config/app/app_jcfg.json",
                     "--output-dir", "../genconfig")
        self.assertEqual(self.read_output("dev/app.json"), {"value": 7})

    def test_help(self):
        result = self.run_cli("--help")
        for option in ("--source", "--input-dir", "--app", "--config",
                       "--output-dir", "--list", "--verbose"):
            self.assertIn(option, result.stdout)
        self.assertEqual(self.files(), {})

    def test_invalid_argument_combinations(self):
        source = self.source({"jcfg": {"name": "dev"}})
        cases = [
            [],
            ["--source", source],
            ["--source", source, "--input-dir", self.inputs, "--list"],
            ["--source", source, "--app", "app", "--list"],
            ["--input-dir", self.inputs, "--config", "dev", "--list"],
            ["--app", "app", "--list"],
            ["--config", "dev", "--list"],
            ["--list"],
            ["--unknown-option"],
        ]
        for arguments in cases:
            with self.subTest(arguments=arguments):
                self.run_cli(*arguments, success=False)
        self.assertEqual(self.files(), {})

    def test_list_resolves_metadata_without_generating(self):
        source = self.source([
            {"jcfg": {"name": "common", "generate": False, "gen_file": "capture.json"},
             "value": 1},
            {"jcfg": {"name": "dev", "extend": "common"}},
            {"jcfg": {"name": "prod", "extend": "dev", "gen_file": "other.json"}},
        ])
        result = self.run_cli("--source", source, "--list")
        self.assertEqual(json.loads(result.stdout), [
            {"app": "app", "section": "common", "gen_file": "capture.json", "generate": False},
            {"app": "app", "section": "dev", "gen_file": "capture.json", "generate": True},
            {"app": "app", "section": "prod", "gen_file": "other.json", "generate": True},
        ])
        self.assertEqual(self.files(), {})

    def test_list_can_select_suppressed_sections(self):
        source = self.source([
            {"jcfg": {"name": "common", "generate": False}},
            {"jcfg": {"name": "prod", "extend": "common"}},
        ])
        result = self.run_cli("--source", source, "--list", "--config", "common")
        self.assertEqual(json.loads(result.stdout), [
            {"app": "app", "section": "common", "gen_file": "app.json", "generate": False},
        ])

    def test_list_does_not_evaluate_imports_or_application_instructions(self):
        source = self.source({
            "jcfg": {"name": "dev", "import": "missing.json"},
            "value": "{jcfg:gen-var JCFG_TEST_MISSING_VALUE}",
            "another": "jcfg:unknown instruction",
        })
        result = self.run_cli("--source", source, "--list")
        self.assertEqual(json.loads(result.stdout)[0]["section"], "dev")
        self.assertEqual(self.files(), {})

    def test_list_validates_all_metadata_and_unknown_selection(self):
        source = self.source([
            {"jcfg": {"name": "dev"}},
            {"jcfg": {"name": "prod", "extend": "missing"}},
        ])
        self.run_cli("--source", source, "--list", "--config", "dev", success=False)
        source = self.source({"jcfg": {"name": "dev"}})
        result = self.run_cli("--source", source, "--list", "--config", "missing", success=False)
        self.assertIn("missing", result.stderr)

    def test_list_uses_source_stem_without_conventional_suffix(self):
        source = self.write_json(self.inputs / "settings.json", {"jcfg": {"name": "dev"}})
        result = self.run_cli("--source", source, "--list")
        self.assertEqual(json.loads(result.stdout), [
            {"app": "settings", "section": "dev", "gen_file": "settings.json", "generate": True},
        ])

    def test_discovery_finds_root_and_nested_sources_by_filename(self):
        self.write_json(self.inputs / "root_jcfg.json", {"jcfg": {"name": "prod"}})
        self.write_json(self.inputs / "z_group/alpha_jcfg.json", {"jcfg": {"name": "dev"}})
        self.write_json(self.inputs / "outer/deep/beta_jcfg.json", {"jcfg": {"name": "dev"}})
        for path in (self.inputs / "donor.json",
                     self.inputs / "outer/other_jcfg.json.backup",
                     self.inputs / "outer/deep/not_a_source.json"):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("invalid JSON", encoding="utf-8")
        self.run_cli("--input-dir", self.inputs, "--output-dir", self.outputs)
        self.assertEqual(set(self.files()), {"dev/alpha.json", "dev/beta.json", "prod/root.json"})
        result = self.run_cli("--input-dir", self.inputs, "--list")
        self.assertEqual([item["app"] for item in json.loads(result.stdout)], ["alpha", "beta", "root"])

    def test_app_selection_matches_filename_exactly_and_does_not_parse_other_apps(self):
        self.write_json(self.inputs / "group/nested/capture_jcfg.json",
                        {"jcfg": {"name": "dev"}, "value": 1})
        for name in ("gateway_jcfg.json", "capture_extra_jcfg.json"):
            (self.inputs / name).write_text("invalid JSON", encoding="utf-8")
        self.run_cli("--input-dir", self.inputs, "--app", "capture",
                     "--config", "dev", "--output-dir", self.outputs)
        self.assertEqual(set(self.files()), {"dev/capture.json"})
        result = self.run_cli("--input-dir", self.inputs, "--app", "capture", "--list")
        self.assertEqual([item["app"] for item in json.loads(result.stdout)], ["capture"])
        self.run_cli("--input-dir", self.inputs, "--app", "cap", "--list", success=False)

    def test_duplicate_app_selection_is_ambiguous_and_reports_all_matching_paths(self):
        first = self.write_json(self.inputs / "capture_jcfg.json", {"jcfg": {"name": "dev"}})
        second = self.write_json(self.inputs / "nested/capture_jcfg.json",
                                 {"jcfg": {"name": "prod"}})
        for options in (("--list",), ("--output-dir", self.outputs)):
            with self.subTest(options=options):
                result = self.run_cli("--input-dir", self.inputs, "--app", "capture",
                                      *options, success=False)
                self.assertIn("ambiguous", result.stderr.lower())
                self.assertIn(str(first), result.stderr)
                self.assertIn(str(second), result.stderr)
                self.assertEqual(self.files(), {})

    def test_all_app_discovery_allows_same_stems_and_sorts_by_app_then_path(self):
        for name, section, value in (("z/worker_jcfg.json", "dev", "last"),
                                     ("a/worker_jcfg.json", "prod", "middle"),
                                     ("q/alpha_jcfg.json", "ci", "first")):
            self.write_json(self.inputs / name, {"jcfg": {"name": section}, "value": value})
        result = self.run_cli("--input-dir", self.inputs, "--list")
        self.assertEqual([(item["app"], item["section"]) for item in json.loads(result.stdout)],
                         [("alpha", "ci"), ("worker", "prod"), ("worker", "dev")])
        self.run_cli("--input-dir", self.inputs, "--output-dir", self.outputs)
        self.assertEqual(set(self.files()), {"ci/alpha.json", "prod/worker.json", "dev/worker.json"})
        for output, value in (("ci/alpha.json", "first"), ("prod/worker.json", "middle"),
                              ("dev/worker.json", "last")):
            self.assertEqual(self.read_output(output), {"value": value})

    def test_same_stem_discovery_still_rejects_conflicting_outputs(self):
        for name in ("first/capture_jcfg.json", "second/capture_jcfg.json"):
            self.write_json(self.inputs / name, {"jcfg": {"name": "dev"}})
        self.run_cli("--input-dir", self.inputs, "--output-dir", self.outputs, success=False)
        self.assertEqual(self.files(), {})

    def test_missing_input_unknown_app_and_empty_discovery(self):
        self.inputs.mkdir()
        for arguments in (["--input-dir", self.inputs],
                          ["--input-dir", self.inputs / "missing"],
                          ["--input-dir", self.inputs, "--app", "missing"],
                          ["--source", self.inputs / "missing_jcfg.json"]):
            with self.subTest(arguments=arguments):
                self.run_cli(*arguments, "--output-dir", self.outputs, success=False)
        self.assertEqual(self.files(), {})

    def test_app_selector_cannot_escape_collection_directory(self):
        self.source({"jcfg": {"name": "dev"}})
        for app in ("../app", "/app", "app/nested"):
            with self.subTest(app=app):
                self.run_cli("--input-dir", self.inputs, "--app", app,
                             "--output-dir", self.outputs, success=False)
        self.assertEqual(self.files(), {})

    def test_repeated_section_selection_resolves_ancestors_without_writing_them(self):
        source = self.source([
            {"jcfg": {"name": "common", "generate": False}, "first": 1},
            {"jcfg": {"name": "ci", "extend": "common"}, "second": 2},
            {"jcfg": {"name": "prod", "extend": "ci"}, "third": 3},
            {"jcfg": {"name": "unused", "extend": "common", "import": "missing.json"}},
        ])
        self.generate(source, "--config", "prod", "--config", "ci")
        self.assertEqual(set(self.files()), {"ci/app.json", "prod/app.json"})
        self.assertEqual(self.read_output("ci/app.json"), {"first": 1, "second": 2})
        self.assertEqual(self.read_output("prod/app.json"), {"first": 1, "second": 2, "third": 3})

    def test_unknown_and_suppressed_sections_cannot_be_generated(self):
        source = self.source([
            {"jcfg": {"name": "common", "generate": False}},
            {"jcfg": {"name": "prod", "extend": "common"}},
        ])
        for section in ("missing", "common"):
            with self.subTest(section=section):
                result = self.generate(source, "--config", "prod", "--config", section, success=False)
                self.assertIn(section, result.stderr)
                self.assertEqual(self.files(), {})

    def test_all_suppressed_sections_succeed_without_evaluating_data(self):
        source = self.source({
            "jcfg": {"name": "common", "generate": False, "import": "missing.json"},
            "value": "{jcfg:gen-var JCFG_TEST_MISSING_VALUE}",
        })
        self.generate(source)
        self.assertEqual(self.files(), {})

    def test_verbose_paths_are_diagnostics_not_list_json(self):
        source = self.source({"jcfg": {"name": "dev"}, "value": 1})
        result = self.run_cli("--source", source, "--list", "--verbose")
        self.assertEqual(len(json.loads(result.stdout)), 1)
        self.assertIn(str(source), result.stderr)
        result = self.generate(source, "--verbose")
        self.assertIn(str(source), result.stderr)
        self.assertIn(str(self.outputs / "dev/app.json"), result.stderr)

    def test_all_documented_expected_outputs(self):
        sources = sorted((source, source.parent / "genconfig")
                         for source in examples.glob("*/*_jcfg.json")
                         if (source.parent / "genconfig").is_dir())
        sources.append((repository / "apps/md_capture/config/md_capture_jcfg.json",
                        examples / "complete_config/genconfig"))
        self.assertEqual(len(sources), 10)
        for source, expected_dir in sources:
            with self.subTest(example=source.parent.name):
                self.outputs = self.directory / "generated_examples" / source.parent.name
                result = self.run_cli("--source", source, "--list")
                metadata = json.loads(result.stdout)
                arguments = [part for item in metadata if item["generate"]
                             for part in ("--config", item["section"])]
                self.generate(source, *arguments)
                expected_files = set(expected_dir.rglob("*.json"))
                matched = set()
                expected_outputs = set()
                for item in metadata:
                    if not item["generate"]:
                        continue
                    output = f"{item['section']}/{item['gen_file']}"
                    expected = expected_dir / output
                    if not expected.is_file():
                        expected = expected_dir / item["gen_file"]
                    self.assertEqual(self.read_output(output), json.loads(expected.read_text()))
                    matched.add(expected)
                    expected_outputs.add(output)
                self.assertEqual(matched, expected_files)
                self.assertEqual(set(self.files()), expected_outputs)

    def test_external_imports_and_environment_placeholders(self):
        library = self.directory / "external_library"
        self.write_json(library / "markets.json", {"spot": ["BTC", "ETH"]})
        self.environment["JCFG_TEST_LIBRARY"] = str(library)
        self.environment["JCFG_TEST_ACCOUNT"] = "1234"
        source = self.source({
            "jcfg": {"name": "prod"},
            "symbols": "jcfg:import {jcfg:gen-var JCFG_TEST_LIBRARY}/markets.json:/spot",
            "account": "{jcfg:gen-var JCFG_TEST_ACCOUNT}",
            "secret": "{jcfg:run-var JCFG_TEST_MISSING_VALUE}",
        })
        self.generate(source)
        self.assertEqual(self.read_output("prod/app.json"), {
            "symbols": ["BTC", "ETH"], "account": "1234",
            "secret": "{jcfg:run-var JCFG_TEST_MISSING_VALUE}",
        })
        self.write_json(library / "markets.json", {"spot": ["SOL"]})
        self.generate(source)
        self.assertEqual(self.read_output("prod/app.json")["symbols"], ["SOL"])

    def test_relative_imports_use_source_directory(self):
        source = self.source({"jcfg": {"name": "dev", "import": "donor.json"}})
        self.write_json(source.parent / "donor.json", {"origin": "source"})
        self.write_json(self.working / "donor.json", {"origin": "working"})
        self.generate(source)
        self.assertEqual(self.read_output("dev/app.json"), {"origin": "source"})

    def test_generation_failure_preserves_all_previous_outputs(self):
        source = self.source([
            {"jcfg": {"name": "dev"}, "value": "old"},
            {"jcfg": {"name": "prod", "extend": "dev"}},
        ])
        self.generate(source)
        previous = self.files()
        self.write_json(source, [
            {"jcfg": {"name": "dev"}, "value": "new"},
            {"jcfg": {"name": "prod", "extend": "dev", "import": "missing.json"}},
        ])
        result = self.generate(source, success=False)
        for detail in (str(source), "prod", "missing.json"):
            self.assertIn(detail, result.stderr)
        self.assertEqual(self.files(), previous)

    def test_validates_entire_multi_app_request_before_publishing(self):
        self.source({"jcfg": {"name": "dev"}, "value": "new"}, "first")
        self.source({"jcfg": {"name": "dev", "import": "missing.json"}}, "second")
        self.write_json(self.outputs / "dev/first.json", {"value": "old"})
        previous = self.files()
        self.run_cli("--input-dir", self.inputs, "--output-dir", self.outputs, success=False)
        self.assertEqual(self.files(), previous)

    def test_invalid_source_and_unset_environment_leave_no_outputs(self):
        source = self.source({"jcfg": {"name": "dev"}})
        for text in ("not JSON", '{"jcfg": {"name": "dev"}, "value": NaN}',
                     '{"jcfg": {"name": "dev"}, "value": "{jcfg:gen-var JCFG_TEST_MISSING_VALUE}"}'):
            with self.subTest(text=text):
                source.write_text(text, encoding="utf-8")
                self.generate(source, success=False)
                self.assertEqual(self.files(), {})

    def test_unchanged_output_preserves_contents_and_modification_time(self):
        source = self.source({"jcfg": {"name": "dev"}, "value": "unchanged"})
        self.generate(source)
        output = self.outputs / "dev/app.json"
        contents = output.read_bytes()
        os.utime(output, ns=(1_600_000_000_000_000_000, 1_600_000_000_000_000_000))
        original_time = output.stat().st_mtime_ns
        self.generate(source)
        self.assertEqual(output.read_bytes(), contents)
        self.assertEqual(output.stat().st_mtime_ns, original_time)
        self.write_json(source, {"jcfg": {"name": "dev"}, "value": "changed"})
        self.generate(source)
        self.assertEqual(self.read_output("dev/app.json"), {"value": "changed"})
        self.assertEqual(set(self.files()), {"dev/app.json"})

    def test_same_output_from_two_apps_is_an_error(self):
        for app in ("capture", "gateway"):
            self.source({"jcfg": {"name": "prod", "gen_file": "shared.json"}}, app)
        self.run_cli("--input-dir", self.inputs, "--output-dir", self.outputs, success=False)
        self.assertEqual(self.files(), {})

    def test_lexically_aliased_output_paths_conflict(self):
        source = self.source([
            {"jcfg": {"name": "prod", "gen_file": "app.json"}},
            {"jcfg": {"name": "prod/.", "extend": "prod", "gen_file": "./app.json"}},
        ])
        self.generate(source, success=False)
        self.assertEqual(self.files(), {})

    def test_symlink_aliases_cannot_make_two_outputs_share_a_path(self):
        source = self.source([
            {"jcfg": {"name": "dev"}, "value": "first"},
            {"jcfg": {"name": "prod", "extend": "dev"}, "value": "second"},
        ])
        (self.outputs / "dev").mkdir(parents=True)
        (self.outputs / "prod").symlink_to(self.outputs / "dev", target_is_directory=True)
        self.generate(source, success=False)
        self.assertEqual(self.files(), {})

    def test_output_directory_and_file_conflicts_are_checked_before_writes(self):
        source = self.source([
            {"jcfg": {"name": "prod", "gen_file": "nested"}},
            {"jcfg": {"name": "prod/nested", "extend": "prod", "gen_file": "app.json"}},
        ])
        self.generate(source, success=False)
        self.assertEqual(self.files(), {})

    def test_existing_non_directory_output_parent_is_an_error(self):
        source = self.source({"jcfg": {"name": "prod", "gen_file": "nested/app.json"}})
        self.write_json(self.outputs / "prod/nested", {"existing": "file"})
        previous = self.files()
        self.generate(source, success=False)
        self.assertEqual(self.files(), previous)

    def test_existing_directory_at_output_file_is_an_error(self):
        source = self.source({"jcfg": {"name": "prod"}})
        (self.outputs / "prod/app.json").mkdir(parents=True)
        self.generate(source, success=False)
        self.assertEqual(self.files(), {})

    def test_output_names_cannot_traverse_or_use_absolute_paths(self):
        for field in ("name", "gen_file"):
            for value in ("../escape.json", "nested/../../escape.json", str(self.directory / "escape.json")):
                with self.subTest(field=field, value=value):
                    metadata = {"name": "dev", field: value}
                    source = self.source({"jcfg": metadata})
                    self.generate(source, success=False)
                    self.assertEqual(self.files(), {})
                    self.assertFalse((self.directory / "escape.json").exists())

    def test_output_root_cannot_overlap_input_directory(self):
        source = self.source({"jcfg": {"name": "dev"}})
        original = source.read_bytes()
        for output in (source.parent, self.inputs, self.directory):
            with self.subTest(output=output):
                self.run_cli("--source", source, "--output-dir", output, success=False)
                self.assertEqual(source.read_bytes(), original)
        self.run_cli("--input-dir", self.inputs, "--output-dir", self.inputs / "generated", success=False)

    def test_source_mode_allows_a_separate_output_tree_below_source_parent(self):
        source = self.source({"jcfg": {"name": "dev"}, "value": "generated"})
        output = source.parent / "build/genconfig"
        self.run_cli("--source", source, "--output-dir", output)
        self.assertEqual(json.loads((output / "dev/app.json").read_text()), {"value": "generated"})

    def test_output_root_cannot_overlap_input_through_symlink(self):
        source = self.source({"jcfg": {"name": "dev"}})
        alias = self.directory / "alias"
        alias.symlink_to(source.parent, target_is_directory=True)
        self.run_cli("--source", source, "--output-dir", alias, success=False)
        self.assertEqual(sorted(path.name for path in source.parent.iterdir()), [source.name])

    def test_symlinked_output_parent_cannot_escape_root(self):
        source = self.source({"jcfg": {"name": "prod"}, "value": "new"})
        outside = self.directory / "outside"
        outside.mkdir()
        self.write_json(outside / "app.json", {"value": "old"})
        self.outputs.mkdir()
        (self.outputs / "prod").symlink_to(outside, target_is_directory=True)
        previous = (outside / "app.json").read_bytes()
        self.generate(source, success=False)
        self.assertEqual((outside / "app.json").read_bytes(), previous)
        self.assertEqual(sorted(path.name for path in outside.iterdir()), ["app.json"])

    def test_symlinked_output_file_cannot_escape_root(self):
        source = self.source({"jcfg": {"name": "prod"}, "value": "new"})
        outside = self.write_json(self.directory / "outside.json", {"value": "old"})
        (self.outputs / "prod").mkdir(parents=True)
        (self.outputs / "prod/app.json").symlink_to(outside)
        previous = outside.read_bytes()
        self.generate(source, success=False)
        self.assertEqual(outside.read_bytes(), previous)
        self.assertTrue((self.outputs / "prod/app.json").is_symlink())

    def test_external_donor_cannot_be_overwritten_as_an_output(self):
        donor = self.write_json(self.outputs / "dev/app.json", {"value": "donor"})
        source = self.source({"jcfg": {"name": "dev", "import": str(donor)}, "value": "changed"})
        previous = self.files()
        self.generate(source, success=False)
        self.assertEqual(self.files(), previous)


if __name__ == "__main__":
    unittest.main()
