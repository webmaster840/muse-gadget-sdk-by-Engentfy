# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MUSE = ROOT / "components/muse"
# CI supplies pinned upstream sources; local IDF builds already have cJSON.
JSON = Path(os.environ.get(
    "CJSON_SOURCE_DIR", ROOT / "managed_components/espressif__cjson/cJSON"
))


class MuseElevenLabsTest(unittest.TestCase):
    def test_request_url_and_body(self):
        with tempfile.TemporaryDirectory() as temp:
            exe = Path(temp) / "elevenlabs"
            cc = shlex.split(os.environ.get("CC", "cc"))
            cmd = [*cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(JSON), "-I", str(MUSE),
                   str(ROOT / "tests/muse_elevenlabs_harness.c"), str(MUSE / "muse_elevenlabs_req.c"),
                   str(JSON / "cJSON.c"), "-lm", "-o", str(exe)]
            built = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_session_hooks_stay_behind_the_option(self):
        session = (MUSE / "muse_chat_session.cpp").read_text()
        for call in ("muse_el_start(", "muse_el_cancel();", "pump_tts();"):
            at = session.index(call)
            self.assertGreater(at, session.rindex("#if CONFIG_MUSE_ELEVENLABS_TTS", 0, at))
        # The key comes from the build's sdkconfig, never a committed file.
        for f in (ROOT / "devices").glob("sdkconfig*"):
            self.assertNotIn("ELEVENLABS_API_KEY", f.read_text(), f.name)


if __name__ == "__main__":
    unittest.main()
