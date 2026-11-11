# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: MIT
"""The educational examples and focused language contracts for jcfg resolution."""

import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.json_config.jcfg import config_error, generate_configs


repository = Path(__file__).resolve().parents[3]
examples = repository / "docs/json_config/examples"


class example_tests(unittest.TestCase):
    def test_all_documented_expected_outputs(self):
        sources = sorted((source, source.parent / "genconfig")
                         for source in examples.glob("*/*_jcfg.json")
                         if (source.parent / "genconfig").is_dir())
        sources.append((repository / "apps/md_capture/config/md_capture_jcfg.json",
                        examples / "complete_config/genconfig"))
        self.assertEqual(len(sources), 10)
        for source, expected_dir in sources:
            with self.subTest(example=source.parent.name):
                expected_files = set(expected_dir.rglob("*.json"))
                actual_files = set()
                for name, result in generate_configs(source, environment={}).items():
                    self.assertEqual(name, result.name)
                    expected = expected_dir / name / result.gen_file
                    if not expected.is_file():
                        expected = expected_dir / result.gen_file
                    self.assertTrue(expected.is_file(), str(expected))
                    self.assertEqual(result.config, json.loads(expected.read_text()))
                    actual_files.add(expected)
                self.assertEqual(actual_files, expected_files)

    def test_structure_example(self):
        source = examples / "stucture_config/app_jcfg.json"
        results = generate_configs(source, environment={})
        self.assertEqual(list(results), ["ci", "prod"])
        self.assertEqual({name: item.gen_file for name, item in results.items()},
                         {"ci": "app.json", "prod": "prod.json"})
        expected = json.loads(source.with_name("app.json").read_text())
        expected["log"]["level"] = "warn"
        self.assertEqual(results["ci"].config, expected)
        expected["subscription"]["symbols"] = json.loads(
            source.with_name("symbol.json").read_text())
        self.assertEqual(results["prod"].config, expected)


class resolver_tests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.directory = Path(directory.name)
        self.source = self.directory / "app_jcfg.json"

    def write_json(self, name, content):
        destination = self.directory / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(content), encoding="utf-8")
        return destination

    def resolve_document(self, content, **options):
        self.write_json(self.source.name, content)
        options.setdefault("environment", {})
        return generate_configs(self.source, **options)

    def assert_failure(self, content, *details, **options):
        with self.assertRaises(config_error) as caught:
            self.resolve_document(content, **options)
        message = str(caught.exception)
        self.assertIn(self.source.name, message)
        for detail in details:
            self.assertIn(detail, message)
        return message

    def test_single_object_and_single_section_array_are_equivalent(self):
        content = {"jcfg": {"name": "simulation"}, "enabled": True}
        for document in (content, [content]):
            with self.subTest(document=document):
                results = self.resolve_document(document)
                self.assertEqual(list(results), ["simulation"])
                result = results["simulation"]
                self.assertEqual((result.name, result.gen_file, result.config),
                                 ("simulation", "app.json", {"enabled": True}))

    def test_jcfg_metadata_name_is_reserved_only_at_section_root(self):
        result = self.resolve_document({
            "jcfg": {"name": "dev"}, "nested": {"jcfg": {"name": "application data"}},
        })["dev"]
        self.assertEqual(result.config, {"nested": {"jcfg": {"name": "application data"}}})

    def test_explicit_output_filename_does_not_follow_import_name(self):
        self.write_json("donor.json", {"value": 1})
        result = self.resolve_document({
            "jcfg": {"name": "dev", "gen_file": "capture.json", "import": "donor.json"}
        })["dev"]
        self.assertEqual(result.gen_file, "capture.json")

    def test_invalid_document_and_section_shapes(self):
        for document in (None, False, "text", 2, [], [1], {}, {"jcfg": []},
                         {"jcfg": {}}, {"jcfg": {"name": 1}}):
            with self.subTest(document=document):
                self.assert_failure(document)

    def test_invalid_inheritance(self):
        documents = [
            [{"jcfg": {"name": "dev", "extend": "dev"}}],
            [{"jcfg": {"name": "dev"}}, {"jcfg": {"name": "dev", "extend": "dev"}}],
            [{"jcfg": {"name": "dev"}}, {"jcfg": {"name": "ci"}}],
            [{"jcfg": {"name": "dev"}}, {"jcfg": {"name": "ci", "extend": "missing"}}],
            [{"jcfg": {"name": "dev"}}, {"jcfg": {"name": "ci", "extend": ["dev"]}}],
            [{"jcfg": {"name": "dev"}}, {"jcfg": {"name": "ci", "extend": "prod"}},
             {"jcfg": {"name": "prod", "extend": "dev"}}],
        ]
        for document in documents:
            with self.subTest(document=document):
                self.assert_failure(document)

    def test_invalid_metadata_types(self):
        for field, value in (("generate", 1), ("generate", "false"),
                             ("gen_file", None), ("import", {}), ("import", [1])):
            with self.subTest(field=field, value=value):
                self.assert_failure({"jcfg": {"name": "dev", field: value}}, field)

    def test_identity_parent_and_output_metadata_are_literal(self):
        for field in ("name", "gen_file", "extend"):
            with self.subTest(field=field):
                content = [{"jcfg": {"name": "dev"}},
                           {"jcfg": {"name": "ci", "extend": "dev"}}]
                content[-1]["jcfg"][field] = "{jcfg:gen-var VALUE}"
                self.assert_failure(content, field, environment={"VALUE": "dev"})

    def test_output_metadata_rejects_absolute_paths_and_traversal(self):
        for field in ("name", "gen_file"):
            for value in ("../outside.json", "/tmp/outside.json", "part/../../outside.json"):
                with self.subTest(field=field, value=value):
                    self.assert_failure({"jcfg": {"name": "dev", field: value}}, field)

    def test_selection_keeps_source_order_and_resolves_ancestors(self):
        document = [
            {"jcfg": {"name": "common", "generate": False, "gen_file": "shared.json"},
             "first": 1},
            {"jcfg": {"name": "ci", "extend": "common"}, "second": 2},
            {"jcfg": {"name": "prod", "extend": "ci"}, "third": 3},
        ]
        results = self.resolve_document(document, sections=["prod", "ci"])
        self.assertEqual(list(results), ["ci", "prod"])
        self.assertEqual({name: item.gen_file for name, item in results.items()},
                         {"ci": "shared.json", "prod": "shared.json"})
        self.assertEqual(results["prod"].config, {"first": 1, "second": 2, "third": 3})
        selected = self.resolve_document(document, sections=["prod"])
        self.assertEqual(list(selected), ["prod"])
        self.assertEqual(selected["prod"], results["prod"])

    def test_explicit_unknown_or_suppressed_selection_fails(self):
        document = {"jcfg": {"name": "common", "generate": False}}
        for selected in ("missing", "common"):
            with self.subTest(selected=selected):
                self.assert_failure(document, selected, sections=[selected])

    def test_no_generating_sections_needs_no_external_values(self):
        self.assertEqual(self.resolve_document({
            "jcfg": {"name": "common", "generate": False, "import": "absent.json"},
            "secret": "{jcfg:gen-var MISSING}",
        }), {})

    def test_unselected_section_semantics_are_not_evaluated(self):
        document = [
            {"jcfg": {"name": "dev"}, "value": 1},
            {"jcfg": {"name": "prod", "extend": "dev", "import": "absent.json"},
             "value": "jcfg:unknown"},
        ]
        results = self.resolve_document(document, sections=["dev"])
        self.assertEqual(list(results), ["dev"])
        self.assertEqual(results["dev"].config, {"value": 1})

    def test_unselected_metadata_is_still_validated(self):
        self.assert_failure([
            {"jcfg": {"name": "dev"}},
            {"jcfg": {"name": "prod", "extend": "missing", "generate": False}},
        ], "prod", sections=["dev"])

    def test_selected_child_evaluates_suppressed_parent(self):
        self.assert_failure([
            {"jcfg": {"name": "common", "generate": False},
             "secret": "{jcfg:gen-var MISSING}"},
            {"jcfg": {"name": "prod", "extend": "common"}},
        ], "common", "secret", "MISSING", sections=["prod"])

    def test_recursive_merge_replaces_other_types_and_preserves_siblings(self):
        results = self.resolve_document([
            {"jcfg": {"name": "dev"}, "object": {"keep": 1, "change": 2},
             "array": [1, 2], "scalar": 3},
            {"jcfg": {"name": "ci", "extend": "dev"},
             "object": {"change": 5}, "array": [6], "scalar": {"new": 7}},
            {"jcfg": {"name": "prod", "extend": "dev"}, "object": 8, "array": None},
        ])
        self.assertEqual(results["dev"].config,
                         {"object": {"keep": 1, "change": 2}, "array": [1, 2], "scalar": 3})
        self.assertEqual(results["ci"].config,
                         {"object": {"keep": 1, "change": 5}, "array": [6],
                          "scalar": {"new": 7}})
        self.assertEqual(results["prod"].config, {"object": 8, "array": None, "scalar": 3})
        results["ci"].config["object"]["keep"] = 99
        self.assertEqual(results["dev"].config["object"]["keep"], 1)

    def test_imports_merge_in_order_then_local_values(self):
        self.write_json("one.json", {"data": {"value": 2, "one": True}})
        self.write_json("two.json", {"data": {"value": 3, "two": True}})
        results = self.resolve_document([
            {"jcfg": {"name": "dev"}, "data": {"value": 1, "parent": True}},
            {"jcfg": {"name": "ci", "extend": "dev", "import": ["one.json", "two.json"]},
             "data": {"value": 4}},
        ])
        self.assertEqual(results["ci"].config["data"],
                         {"value": 4, "parent": True, "one": True, "two": True})

    def test_section_import_requires_object(self):
        for donor in ([1], "text", 2, None):
            with self.subTest(donor=donor):
                self.write_json("donor.json", donor)
                self.assert_failure({"jcfg": {"name": "dev", "import": "donor.json"}},
                                    "dev", "donor.json")

    def test_property_import_merges_object_and_retains_other_json_types(self):
        self.write_json("donor.json", {"object": {"new": 2}, "array": [3],
                                       "string": "text", "number": 4, "null": None,
                                       "boolean": False})
        results = self.resolve_document([
            {"jcfg": {"name": "dev"}, "object": {"keep": 1}},
            {"jcfg": {"name": "ci", "extend": "dev"},
             **{key: f"jcfg:import donor.json:/{key}"
                for key in ("object", "array", "string", "number", "null", "boolean")}},
        ])
        self.assertEqual(results["ci"].config,
                         {"object": {"keep": 1, "new": 2}, "array": [3], "string": "text",
                          "number": 4, "null": None, "boolean": False})

    def test_imported_json_instructions_and_placeholders_remain_literal(self):
        literal = {"remove": "jcfg:delete", "replace": {"jcfg:override": {"new": 2}},
                   "generation": "{jcfg:gen-var MISSING}",
                   "runtime": "{jcfg:run-var MISSING}", "import": "jcfg:import absent.json"}
        self.write_json("donor.json", literal)
        result = self.resolve_document({
            "jcfg": {"name": "dev", "import": "donor.json"},
            "nested": "jcfg:import donor.json",
        })["dev"]
        self.assertEqual(result.config, {**literal, "nested": literal})

    def test_imported_override_wrapper_is_literal_during_recursive_merge(self):
        self.write_json("donor.json", {"node": {"jcfg:override": {"new": 2}}})
        results = self.resolve_document([
            {"jcfg": {"name": "dev"}, "node": {"keep": 1}},
            {"jcfg": {"name": "ci", "extend": "dev", "import": "donor.json"}},
        ])
        self.assertEqual(results["ci"].config["node"],
                         {"keep": 1, "jcfg:override": {"new": 2}})

    def test_import_pointer_escapes_empty_key_arrays_and_colons(self):
        self.write_json("archive.json/markets.json", {
            "a/b": {"~key": [{"": {"venue:spot": 42}}]}, "~1": "literal slash escape",
        })
        result = self.resolve_document({
            "jcfg": {"name": "dev"},
            "value": "jcfg:import archive.json/markets.json:/a~1b/~0key/0//venue:spot",
            "escape_order": "jcfg:import archive.json/markets.json:/~01",
        })["dev"]
        self.assertEqual(result.config, {"value": 42, "escape_order": "literal slash escape"})

    def test_invalid_pointers_fail(self):
        self.write_json("donor.json", {"items": ["one"], "scalar": 2})
        for pointer in ("missing_slash", "/absent", "/items/1", "/items/01", "/items/-",
                        "/items/-1", "/items/a", "/scalar/child", "/bad~2escape"):
            with self.subTest(pointer=pointer):
                self.assert_failure({"jcfg": {"name": "dev"},
                                     "value": f"jcfg:import donor.json:{pointer}"},
                                    "dev", "value", "donor.json")

    def test_relative_and_absolute_import_paths_use_source_location(self):
        donor = self.write_json("library/donor.json", [1, 2])
        result = self.resolve_document({
            "jcfg": {"name": "dev"}, "relative": "jcfg:import library/donor.json",
            "absolute": f"jcfg:import {donor}",
        })["dev"]
        self.assertEqual(result.config, {"relative": [1, 2], "absolute": [1, 2]})

    def test_import_path_can_contain_spaces(self):
        self.write_json("config library/market data.json", {"name": "BTC"})
        result = self.resolve_document({
            "jcfg": {"name": "dev"},
            "symbol": "jcfg:import config library/market data.json:/name",
        })["dev"]
        self.assertEqual(result.config, {"symbol": "BTC"})

    def test_delete_removes_property_but_null_is_data(self):
        results = self.resolve_document([
            {"jcfg": {"name": "dev"}, "node": {"keep": 1, "remove": 2}, "null": 3},
            {"jcfg": {"name": "ci", "extend": "dev"},
             "node": {"remove": "jcfg:delete"}, "null": None},
        ])
        self.assertEqual(results["ci"].config, {"node": {"keep": 1}, "null": None})
        self.assertEqual(results["dev"].config["node"]["remove"], 2)

    def test_delete_missing_property_and_array_element_fail(self):
        for content in ({"missing": "jcfg:delete"}, {"array": ["jcfg:delete"]},
                        {"node": {"missing": "jcfg:delete"}}):
            with self.subTest(content=content):
                self.assert_failure({"jcfg": {"name": "dev"}, **content}, "dev")

    def test_override_replaces_whole_value_and_creates_missing_property(self):
        for replacement in ({"new": 2}, [3], "text", 4, None):
            with self.subTest(replacement=replacement):
                results = self.resolve_document([
                    {"jcfg": {"name": "dev"}, "existing": {"old": 1}},
                    {"jcfg": {"name": "ci", "extend": "dev"},
                     "existing": {"jcfg:override": replacement},
                     "absent": {"jcfg:override": replacement}},
                ])
                self.assertEqual(results["ci"].config,
                                 {"existing": replacement, "absent": replacement})

    def test_override_resolves_nested_imports_without_inherited_content(self):
        self.write_json("donor.json", ["BTC"])
        results = self.resolve_document([
            {"jcfg": {"name": "dev"}, "subscription": {"old": True}},
            {"jcfg": {"name": "ci", "extend": "dev"},
             "subscription": {"jcfg:override": {"symbols": "jcfg:import donor.json"}}},
        ])
        self.assertEqual(results["ci"].config, {"subscription": {"symbols": ["BTC"]}})

    def test_malformed_override_and_unknown_instruction_fail(self):
        for value in ({"jcfg:override": {}, "extra": 1}, "jcfg:unknown value",
                      {"jcfg:unknown": 1}, "jcfg:import"):
            with self.subTest(value=value):
                self.assert_failure({"jcfg": {"name": "dev"}, "value": value},
                                    "dev", "value")

    def test_generation_substitution_preserves_text_and_string_type(self):
        result = self.resolve_document({
            "jcfg": {"name": "dev"},
            "value": "{jcfg:gen-var COUNT}",
            "path": "{jcfg:gen-var ROOT}/{jcfg:gen-var NAME}/{jcfg:gen-var NAME}",
            "nested": [{"value": "{jcfg:gen-var EMPTY}"}],
            "runtime": "{jcfg:run-var UNSET}",
            "both": "{jcfg:gen-var ROOT}/{jcfg:run-var UNSET}",
        }, environment={"COUNT": "42", "ROOT": "/srv", "NAME": "data", "EMPTY": ""})["dev"]
        self.assertEqual(result.config,
                         {"value": "42", "path": "/srv/data/data", "nested": [{"value": ""}],
                          "runtime": "{jcfg:run-var UNSET}",
                          "both": "/srv/{jcfg:run-var UNSET}"})

    def test_generation_placeholders_in_section_and_property_imports(self):
        donor = self.write_json("library/donor.json", {"value": 7})
        result = self.resolve_document({
            "jcfg": {"name": "dev", "import": "{jcfg:gen-var LIB}/donor.json"},
            "other": "jcfg:import {jcfg:gen-var LIB}/donor.json:/value",
        }, environment={"LIB": str(donor.parent)})["dev"]
        self.assertEqual(result.config, {"value": 7, "other": 7})

    def test_substitution_does_not_interpret_inserted_text(self):
        for text in ("{jcfg:gen-var MISSING}", "jcfg:delete", "jcfg:import absent.json"):
            with self.subTest(text=text):
                result = self.resolve_document({
                    "jcfg": {"name": "dev"}, "value": "{jcfg:gen-var TEXT}",
                }, environment={"TEXT": text})["dev"]
                self.assertEqual(result.config, {"value": text})

    def test_missing_generation_variable_fails_with_context(self):
        self.assert_failure({"jcfg": {"name": "dev"},
                             "connection": {"token": "{jcfg:gen-var MISSING}"}},
                            "dev", "connection", "token", "MISSING")

    def test_malformed_placeholders_fail_in_authored_values(self):
        for value in ("{jcfg:gen-var}", "{jcfg:gen-var NAME", "{jcfg:run-var}",
                      "{jcfg:unknown NAME}"):
            with self.subTest(value=value):
                self.assert_failure({"jcfg": {"name": "dev"}, "value": value},
                                    "dev", "value")

    def test_runtime_placeholders_cannot_resolve_import_paths(self):
        for content in (
            {"jcfg": {"name": "dev", "import": "{jcfg:run-var LIB}/donor.json"}},
            {"jcfg": {"name": "dev"},
             "value": "jcfg:import {jcfg:run-var LIB}/donor.json"},
        ):
            with self.subTest(content=content):
                self.assert_failure(content, "dev", environment={"LIB": str(self.directory)})

    def test_default_environment_and_explicit_environment_are_distinct(self):
        content = {"jcfg": {"name": "dev"}, "value": "{jcfg:gen-var JCFG_TEST_VALUE}"}
        self.write_json(self.source.name, content)
        with patch.dict("os.environ", {"JCFG_TEST_VALUE": "from environment"}, clear=True):
            result = generate_configs(str(self.source))["dev"]
            self.assertEqual(result.config["value"], "from environment")
            self.assert_failure(content, "JCFG_TEST_VALUE", environment={})
            result = generate_configs(self.source, environment={"JCFG_TEST_VALUE": "explicit"})["dev"]
            self.assertEqual(result.config["value"], "explicit")

    def test_missing_import_reports_source_section_property_and_donor(self):
        self.assert_failure({"jcfg": {"name": "dev"},
                             "connection": "jcfg:import missing.json"},
                            "dev", "connection", "missing.json")

    def test_invalid_source_json_is_config_error(self):
        self.source.write_text('{"jcfg":', encoding="utf-8")
        with self.assertRaises(config_error) as caught:
            generate_configs(self.source, environment={})
        self.assertIn(self.source.name, str(caught.exception))

    def test_invalid_donor_json_reports_import_context(self):
        self.source.with_name("donor.json").write_text('{"missing":', encoding="utf-8")
        self.assert_failure({"jcfg": {"name": "dev"}, "value": "jcfg:import donor.json"},
                            "dev", "value", "donor.json")

    def test_missing_source_is_config_error(self):
        with self.assertRaises(config_error) as caught:
            generate_configs(self.source, environment={})
        self.assertIn(self.source.name, str(caught.exception))

    def test_resolution_does_not_write_or_change_files(self):
        self.write_json("donor.json", {"value": 1})
        self.write_json(self.source.name, {"jcfg": {"name": "dev", "import": "donor.json"}})
        before = {path: path.read_bytes() for path in self.directory.rglob("*") if path.is_file()}
        generate_configs(self.source, environment={})
        after = {path: path.read_bytes() for path in self.directory.rglob("*") if path.is_file()}
        self.assertEqual(after, before)


if __name__ == "__main__":
    unittest.main()
