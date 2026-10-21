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

"""display.show_text: Muse leaves a note on the screen (muse_ui_show_note). Its
pages are tested with the caption wrap (test_muse_caption_wrap.py)."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def gated(text, needle):
    """The #if ... #endif block around needle."""
    at = text.index(needle)
    return text[text.rindex("#if ", 0, at):text.index("#endif", at)]


class ShowTextTest(unittest.TestCase):
    def test_command_is_advertised_and_dispatched_with_the_muse_display(self):
        noise = (ROOT / "main/noise_control.cpp").read_text()
        self.assertTrue(gated(noise, '"display.show_text"').startswith("#if CONFIG_HOMEHUB_LED_BACKEND_MUSE\n"))
        app = (ROOT / "main/app.c").read_text()
        dispatch = app[app.index("static cJSON *on_ws_command("):]
        block = gated(dispatch[:dispatch.index("unsupported command")], '"display.show_text"')
        self.assertTrue(block.startswith("#if CONFIG_HOMEHUB_LED_BACKEND_MUSE\n"))
        self.assertIn("muse_ui_show_note(", block)

    def test_muse_is_told_the_limit_the_device_keeps(self):
        header = (ROOT / "components/muse/muse_ui.h").read_text()
        limit = int(re.search(r"#define MUSE_NOTE_MAX (\d+)", header).group(1)) - 1
        noise = (ROOT / "main/noise_control.cpp").read_text()
        app = (ROOT / "main/app.c").read_text()
        self.assertIn(f"Up to {limit} bytes", noise[noise.index("cJSON *text_required"):])
        self.assertIn(f"longer than {limit} bytes", app)


if __name__ == "__main__":
    unittest.main()
