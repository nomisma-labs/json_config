# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary
"""Check application arguments remaining after the framework's config parser."""

import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class config_args_example_tests(unittest.TestCase):
    def setUp(self):
        self.executable = os.environ["JCFG_APP_TEST_EXECUTABLE"]
        directory = tempfile.TemporaryDirectory(prefix="jcfg_app_cli_test_")
        self.addCleanup(directory.cleanup)
        self.directory = Path(directory.name)
        self.config = self.write_config("app.json", {"greeting": "Hello"})
        self.environment = dict(os.environ)
        self.environment.pop("NOM_CONFIG", None)
        self.environment.pop("NOM_CONFIG_DIR", None)

    def write_config(self, name, document):
        path = self.directory / name
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def run_app(self, *arguments, success=True):
        result = subprocess.run(
            [self.executable, *map(str, arguments)],
            cwd=self.directory, env=self.environment,
            capture_output=True, text=True, check=False, timeout=20,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertTrue(result.stderr.strip(), "failure should explain its cause")
        return result

    def test_application_defaults(self):
        result = self.run_app("--config", self.config)
        self.assertEqual(result.stdout, "Hello, world!\n")

    def test_interleaved_config_and_application_options(self):
        result = self.run_app(
            "--name", "Alex", "--config", self.config,
            "--count", "2", "--set", "/greeting=Welcome", "--verbose",
        )
        self.assertEqual(result.stdout, "Greeting count: 2\nWelcome, Alex!\nWelcome, Alex!\n")

    def test_adjacent_option_values(self):
        result = self.run_app(
            f"--config={self.config}", "--name=Alex", "--set=/greeting=Hi", "--count=2",
        )
        self.assertEqual(result.stdout, "Hi, Alex!\nHi, Alex!\n")

    def test_short_application_flag_and_positional_name(self):
        result = self.run_app("--config", self.config, "-v", "Alex")
        self.assertEqual(result.stdout, "Greeting count: 1\nHello, Alex!\n")

    def test_marker_passes_config_looking_name_to_application(self):
        result = self.run_app("--config", self.config, "--", "--set")
        self.assertEqual(result.stdout, "Hello, --set!\n")

    def test_help_precedes_config_loading(self):
        malformed = self.directory / "malformed.json"
        malformed.write_text("{", encoding="utf-8")
        for config in (self.directory / "missing.json", malformed):
            for help_option in ("--help", "-h"):
                with self.subTest(config=config, help_option=help_option):
                    result = self.run_app("--config", config, help_option)
                    for option in ("--config", "--set", "--check-config", "--name", "--count", "--verbose"):
                        self.assertIn(option, result.stdout)
                    self.assertNotIn("Hello,", result.stdout)
                    self.assertEqual(result.stderr, "")

    def test_application_errors_precede_config_loading(self):
        missing = self.directory / "missing.json"
        cases = [
            ["--unknown-app-option"],
            ["--count", "0"],
            ["--count", "-1"],
            ["--count", "many"],
            ["--name="],
            ["Alex", "Sam"],
        ]
        for arguments in cases:
            with self.subTest(arguments=arguments):
                result = self.run_app("--config", missing, *arguments, success=False)
                self.assertNotIn(str(missing), result.stderr)
                self.assertNotIn("Config location", result.stderr)
                self.assertEqual(result.stdout, "")

    def test_check_config_exits_before_application_work(self):
        result = self.run_app(
            "--config", self.config, "--name", "Alex", "--verbose",
            "--set", "/greeting=Hi", "--check-config",
        )
        self.assertEqual(result.stdout, "Config OK\n")

    def test_config_failures_still_reach_framework_diagnostics(self):
        malformed = self.directory / "malformed.json"
        malformed.write_text("{", encoding="utf-8")
        result = self.run_app("--config", malformed, "--name", "Alex", success=False)
        self.assertIn("Config parsing", result.stderr)
        self.assertIn(str(malformed), result.stderr)
        self.assertEqual(result.stdout, "")


if __name__ == "__main__":
    unittest.main()
