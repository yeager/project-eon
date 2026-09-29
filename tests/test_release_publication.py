"""Publication fails closed before creating a release when prerequisites fail."""
import importlib.util
import os
from pathlib import Path
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    "publish_release", Path(__file__).resolve().parents[1] / "packaging/publish-release.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ReleasePublicationTests(unittest.TestCase):
    def setUp(self):
        self.environment = patch.dict(os.environ, {
            "GITHUB_EVENT_NAME": "workflow_dispatch",
            "GITHUB_REPOSITORY": "yeager/project-eon",
            "RELEASE_VERSION": "0.1.0", "RELEASE_BUILD_RUN": "123",
        }, clear=True)
        self.environment.start()
        self.addCleanup(self.environment.stop)
        self.build = {
            "head_repository": {"full_name": "yeager/project-eon"},
            "head_branch": "main", "path": ".github/workflows/build.yml",
            "event": "workflow_dispatch", "status": "completed",
            "conclusion": "success", "head_sha": "a" * 40,
        }

    def test_nonmanual_event_cannot_publish(self):
        os.environ["GITHUB_EVENT_NAME"] = "push"
        with patch.object(MODULE, "api") as api, self.assertRaisesRegex(RuntimeError, "manual"):
            MODULE.main()
        api.assert_not_called()

    def test_version_cannot_inject_shell_or_arguments(self):
        os.environ["RELEASE_VERSION"] = "0.1.0; echo unsafe"
        with patch.object(MODULE, "api") as api, self.assertRaisesRegex(RuntimeError, "version"):
            MODULE.main()
        api.assert_not_called()

    def test_foreign_build_cannot_publish(self):
        self.build["head_repository"]["full_name"] = "other/repo"
        with patch.object(MODULE, "api", return_value=self.build), \
                self.assertRaisesRegex(RuntimeError, "repository"):
            MODULE.main()

    def test_failed_build_cannot_publish(self):
        self.build["conclusion"] = "failure"
        with patch.object(MODULE, "api", return_value=self.build), \
                self.assertRaisesRegex(RuntimeError, "did not succeed"):
            MODULE.main()

    def test_missing_job_cannot_publish(self):
        with patch.object(MODULE, "api", side_effect=[self.build, {"jobs": []}]), \
                self.assertRaisesRegex(RuntimeError, "Every required"):
            MODULE.main()

    def test_missing_packages_cannot_publish(self):
        jobs = {"jobs": [{"name": name, "conclusion": "success"} for name in MODULE.JOBS]}
        with patch.object(MODULE, "api", side_effect=[self.build, jobs, {"artifacts": []}]), \
                self.assertRaisesRegex(RuntimeError, "Complete"):
            MODULE.main()

    def test_gitleaks_report_is_not_a_release_package(self):
        packages = [{"name": name, "expired": False} for name in MODULE.ARTIFACTS]
        MODULE.check_artifacts(packages)
        MODULE.check_artifacts(packages + [{"name": "gitleaks-results.sarif", "expired": False}])
        with self.assertRaises(RuntimeError):
            MODULE.check_artifacts(packages + [{"name": "unexpected", "expired": False}])
        with self.assertRaises(RuntimeError):
            MODULE.check_artifacts(packages + [packages[0]])
        packages[0]["expired"] = True
        with self.assertRaises(RuntimeError):
            MODULE.check_artifacts(packages)
