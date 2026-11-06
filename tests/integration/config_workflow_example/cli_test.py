# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary
"""Generate the sample configs and exercise the real application's startup boundary."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


repository = Path(__file__).resolve().parents[3]


class config_workflow_example_tests(unittest.TestCase):
    def setUp(self):
        self.executable = os.environ["JCFG_WORKFLOW_TEST_EXECUTABLE"]
        directory = tempfile.TemporaryDirectory(prefix="jcfg_workflow_test_")
        self.addCleanup(directory.cleanup)
        self.directory = Path(directory.name)
        self.generated = self.directory / "genconfig"
        self.environment = dict(os.environ)
        self.environment.pop("NOM_CONFIG", None)
        self.environment.pop("NOM_CONFIG_DIR", None)

        result = subprocess.run(
            [sys.executable, str(repository / "tools/json_config/jcfggen.py"),
             "--source", str(repository / "apps/config_workflow_example/config/config_workflow_example_jcfg.json"),
             "--output-dir", str(self.generated)],
            cwd=self.directory, env=self.environment,
            capture_output=True, text=True, check=False, timeout=20,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def config_path(self, section):
        return self.generated / section / "config_workflow_example.json"

    def run_app(self, *arguments, success=True):
        result = subprocess.run(
            [self.executable, *map(str, arguments)],
            cwd=self.directory, env=self.environment,
            capture_output=True, text=True, check=False, timeout=20,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr, "")
        else:
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertEqual(result.stdout, "", "failed config must not initialize the application")
            self.assertTrue(result.stderr.strip(), "failure should explain its cause")
        return result

    def test_generated_sections_initialize_with_their_selected_values(self):
        for section, timeout in (("dev", 10), ("prod", 30)):
            with self.subTest(section=section):
                config = self.config_path(section)
                result = self.run_app("--config", config)
                self.assertEqual(result.stdout,
                                 f"Config OK (command line): {config}\n"
                                 f"Application initialized; timeout: {timeout} seconds\n")

    def test_check_config_exits_before_application_initialization(self):
        for section in ("dev", "prod"):
            with self.subTest(section=section):
                config = self.config_path(section)
                result = self.run_app("--config", config, "--check-config")
                self.assertEqual(result.stdout, f"Config OK (command line): {config}\n")

    def test_generated_section_is_found_through_the_config_key(self):
        self.environment["NOM_CONFIG_DIR"] = str(self.generated / "prod")
        result = self.run_app("--check-config")
        self.assertEqual(result.stdout, f"Config OK (NOM_CONFIG_DIR): {self.config_path('prod')}\n")

    def test_command_line_override_reaches_application_initialization(self):
        result = self.run_app("--config", self.config_path("prod"), "--set", "/timeout_s=20")
        self.assertIn("Application initialized; timeout: 20 seconds\n", result.stdout)

    def test_mapping_failure_reports_the_file_pointer_and_value(self):
        config = self.config_path("prod")
        document = json.loads(config.read_text(encoding="utf-8"))
        document["timeout_s"] = 1.5
        config.write_text(json.dumps(document), encoding="utf-8")
        result = self.run_app("--config", config, "--check-config", success=False)
        for detail in ("Config mapping", str(config), "/timeout_s", "1.5"):
            self.assertIn(detail, result.stderr)

    def test_validation_failure_prevents_both_check_and_normal_startup(self):
        config = self.config_path("prod")
        for arguments in ((), ("--check-config",)):
            with self.subTest(arguments=arguments):
                result = self.run_app("--config", config, "--set", "/timeout_s=0",
                                      *arguments, success=False)
                for detail in ("Config validation", str(config), "timeout_s must be positive"):
                    self.assertIn(detail, result.stderr)


if __name__ == "__main__":
    unittest.main()
