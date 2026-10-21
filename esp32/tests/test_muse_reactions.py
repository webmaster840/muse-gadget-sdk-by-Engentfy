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
import re
import shlex
import subprocess
import tempfile
import unittest
import os

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / "components/muse/muse_state.c"


class MuseReactionsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        out = Path(cls.temp.name)
        source = STATE.read_text()
        start = source.index("/* ---- Reactions (host-tested")
        (out / "reactions.inc").write_text(source[start:source.index("/* ---- end of reactions", start)])
        cc = shlex.split(os.environ.get("CC", "cc"))
        cmd = [*cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "components/muse"), "-I", str(out),
               str(ROOT / "tests/muse_reactions_harness.c"), "-o", str(out / "reactions")]
        compiled = subprocess.run(cmd, capture_output=True, text=True)
        if compiled.returncode:
            raise AssertionError(compiled.stdout + compiled.stderr)
        cls.binary = out / "reactions"

    def test_when_reactions_show_and_drop(self):
        result = subprocess.run([str(self.binary)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_command_is_advertised_and_dispatched_with_the_muse_display(self):
        def gated(text, needle):   # the #if ... #endif around needle
            at = text.index(needle)
            return text[text.rindex("#if ", 0, at):text.index("#endif", at)]

        noise = (ROOT / "main/noise_control.cpp").read_text()
        self.assertTrue(gated(noise, '"avatar.react"').startswith("#if CONFIG_HOMEHUB_LED_BACKEND_MUSE\n"))
        app = (ROOT / "main/app.c").read_text()
        dispatch = app[app.index("static cJSON *on_ws_command("):]
        block = gated(dispatch[:dispatch.index("unsupported command")], '"avatar.react"')
        self.assertTrue(block.startswith("#if CONFIG_HOMEHUB_LED_BACKEND_MUSE\n"))
        self.assertIn("muse_state_react(", block)

    def test_muse_is_told_every_reaction(self):
        names = re.findall(r'\[MUSE_REACTION_\w+\] = "(\w+)"', STATE.read_text())
        self.assertEqual(names, ["happy", "thinking", "shrug", "goodnight"])
        noise = (ROOT / "main/noise_control.cpp").read_text()
        advertised = noise[noise.index("cJSON *react_required"):noise.index("react_required, nullptr);")]
        app = (ROOT / "main/app.c").read_text()
        refusal = app[app.index('"reaction must be'):]
        refusal = refusal[:refusal.index("\n")]
        for name in names:
            self.assertIn(name, advertised)
            self.assertIn(name, refusal)


if __name__ == "__main__":
    unittest.main()
