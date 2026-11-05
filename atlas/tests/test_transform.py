#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests of atlas/transform on tiny trees. The grit parts need the engine's tools/grit: set
ATLAS_ENGINE_TREE to a source tree (any state), otherwise they are skipped and said so."""

import contextlib
import io
import os
import re
import shutil
import sys

sys.dont_write_bytecode = True  # the tools are run with -I from a checkout: leave no __pycache__
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / "atlas" / "transform"))

import _lib  # noqa: E402
import blocker  # noqa: E402
import brand  # noqa: E402
import hosts  # noqa: E402
import scheme  # noqa: E402

ENGINE = os.environ.get("ATLAS_ENGINE_TREE")


def fixture(files):
    d = Path(tempfile.mkdtemp(prefix="atlas-transform."))
    (d / "OWNERS").write_text("x\n")
    (d / "chrome").mkdir()
    for rel, text in files.items():
        p = d / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")
    return d


class NameRule(unittest.TestCase):
    def sub(self, s):
        return brand.NAME.sub("Atlas", s)

    def test_words(self):
        self.assertEqual(self.sub("Helium"), "Atlas")
        self.assertEqual(self.sub("Helium's tabs"), "Atlas's tabs")
        self.assertEqual(self.sub("Helium services"), "Atlas services")
        self.assertEqual(self.sub("[Helium Noise] x"), "[Atlas Noise] x")

    def test_identifiers_stay(self):
        for s in ("kHeliumLayout", "HeliumLayoutType", "prefs::kHeliumNoiseEnabled", "helium.services.enabled",
                  "settings-helium-services-page", "HELIUM_PRODUCT_VERSION", "Heliumx"):
            self.assertEqual(self.sub(s), s)


class SchemeRule(unittest.TestCase):
    def test_url(self):
        rx = scheme.GRIT_RULES[0][1]
        self.assertEqual(rx.sub("atlas://", "go to helium://settings now"), "go to atlas://settings now")
        self.assertEqual(rx.sub("atlas://", "<a href='helium://password-manager/'>"), "<a href='atlas://password-manager/'>")
        self.assertEqual(rx.sub("atlas://", "x.helium://"), "x.helium://")
        self.assertEqual(rx.sub("atlas://", "ku-helium://policy"), "ku-atlas://policy")

    def test_constant(self):
        d = fixture({"content/public/common/url_constants.h": 'inline constexpr char kHeliumUIScheme[] = "helium";  // x\n'})
        r = _lib.Result()
        _lib.text_rules(d, [t for t in scheme.TEXT_RULES if t[1].endswith("url_constants.h")], True, r)
        self.assertEqual(r.counts["scheme.constant.content"], 1)
        self.assertIn('kHeliumUIScheme[] = "atlas";', (d / "content/public/common/url_constants.h").read_text())
        shutil.rmtree(d)


class HostRules(unittest.TestCase):
    def test_every_host_rule_rewrites_its_line(self):
        samples = {
            "hosts.services_base": ('const char kHeliumDefaultOrigin[] =\n    "https://services.helium.imput.net";\n', "127.0.0.3"),
            "hosts.dummy_origin": ('const char kHeliumDummyOrigin[] =\n    "https://helium-services-are-disabled.qjz9zk";\n', "127.0.0.2"),
            "hosts.update_base": ('constexpr std::string_view kUpdateBaseUrl = "https://updates.helium.computer/";\n', 'kUpdateBaseUrl = ""'),
            "hosts.crash_url": ('  return "https://crash.helium.computer/crash";\n', "std::string()"),
            "hosts.extension_ping": ('  downloader->set_ping_enabled_domain("helium.computer");\n', 'set_ping_enabled_domain("")'),
            "hosts.crash_page_bug": ("  let href = 'https://github.com/imputnet/helium/issues/new';\n", "'about:blank'"),
            "hosts.services_placeholder": ('<cr-input placeholder="https://services.helium.imput.net">\n', "example.org"),
            "hosts.blocker_about": ('<a href="https://helium.computer/privacy">x</a><a href="https://github.com/imputnet/uBlock">y</a>\n', 'href="about:blank"'),
        }
        for key, rel, rx, repl in hosts.TEXT_RULES:
            text, expect = samples[key]
            new, n = rx.subn(repl, text)
            self.assertGreaterEqual(n, 1, key)
            self.assertIn(expect, new, key)
            self.assertNotRegex(new, r"helium\.computer|imput\.net|imputnet|helium-services-are-disabled", key)

    def test_loopback_and_unsafe_port(self):
        for url in (hosts.SERVICES, hosts.DUMMY):
            m = re.fullmatch(r"https://(127\.0\.0\.\d+):(\d+)", url)
            self.assertTrue(m, url)
            self.assertIn(m.group(2), {"1", "7", "9", "11", "13", "15", "17", "19", "20", "21", "22", "23", "25"})
        self.assertNotEqual(hosts.SERVICES, hosts.DUMMY)  # the download UI compares the host with the dummy host


class BlockerRule(unittest.TestCase):
    def test_entry_is_removed_alone(self):
        t = ('\t"x": {\n\t\t"a": 1\n\t},\n\t"plowe-0": {\n\t\t"content": "filters",\n\t\t"contentURL": [\n\t\t\t"a",\n'
             '\t\t\t"b"\n\t\t],\n\t\t"supportURL": "u"\n\t},\n\t"ALB-0": {\n\t\t"b": 2\n\t},\n')
        new, n = blocker.ENTRY.subn("", t)
        self.assertEqual(n, 1)
        self.assertEqual(new, '\t"x": {\n\t\t"a": 1\n\t},\n\t"ALB-0": {\n\t\t"b": 2\n\t},\n')


class Counts(unittest.TestCase):
    def test_zero_matches_fail_and_changed_counts_warn(self):
        class A:
            strict = False
            update_counts = False
        r = _lib.Result()
        with contextlib.redirect_stdout(io.StringIO()) as out:
            r.counts = {"hosts.crash_url": 0}
            self.assertEqual(_lib.judge(r, ("hosts.",), A), 1)
            self.assertIn("no match", out.getvalue())
            r.counts = {"hosts.crash_url": 5}
            self.assertEqual(_lib.judge(r, ("hosts.crash_url",), A), 0)
            A.strict = True
            self.assertEqual(_lib.judge(r, ("hosts.crash_url",), A), 1)

    def test_every_rule_has_a_recorded_count(self):
        exp = _lib.load_counts()["expected"]
        for key, *_ in brand.TEXT_RULES + scheme.TEXT_RULES + hosts.TEXT_RULES:
            self.assertIn(key, exp)


@unittest.skipUnless(ENGINE, "ATLAS_ENGINE_TREE not set: grit parts skipped")
class GritPass(unittest.TestCase):
    def test_messages_and_translations_move_together(self):
        from _grit import grit_pass  # pylint: disable=import-outside-toplevel
        grd = ('<?xml version="1.0" encoding="utf-8"?>\n<grit-part>\n'
               '<message name="IDS_A" desc="d">Welcome to Helium and helium://settings</message>\n'
               '<message name="IDS_B" desc="d">Nothing here</message>\n</grit-part>\n')
        bare = grd.replace('<?xml version="1.0" encoding="utf-8"?>\n', "")
        d = fixture({"chrome/app/x.grdp": grd, "chrome/app/y.grdp": bare})
        os.symlink(Path(ENGINE) / "tools", d / "tools")
        sys.path.insert(0, str(ROOT / "utils"))
        import name_substitution_utils as u  # pylint: disable=import-outside-toplevel
        u.add_grit_to_path(Path(d))
        import xml.etree.ElementTree as xml  # pylint: disable=import-outside-toplevel
        msg = xml.fromstring(grd).find("message")
        old = u.compute_fp(msg)
        xtb = ('<?xml version="1.0" encoding="utf-8"?>\n<translationbundle lang="de">\n'
               f'<translation id="{old}">Willkommen bei Helium</translation>\n'
               '<translation id="111">unrelated</translation>\n</translationbundle>\n')
        (d / "chrome/app/x_de.xtb").write_text(xtb, encoding="utf-8")
        rules = brand.GRIT_RULES + [("url", re.compile(r"(?<![\w.])helium://"), "atlas://")]
        counts = grit_pass(d, rules, True, "t")
        self.assertEqual(counts["t.grit.messages"], 2)
        self.assertTrue((d / "chrome/app/x.grdp").read_text().startswith("<?xml"))
        self.assertTrue((d / "chrome/app/y.grdp").read_text().startswith("<grit-part>"))  # no new declaration
        self.assertEqual(counts["t.xtb.translations"], 1)
        new = xml.fromstring((d / "chrome/app/x.grdp").read_text()).find("message")
        self.assertEqual(new.text, "Welcome to Atlas and atlas://settings")
        text = (d / "chrome/app/x_de.xtb").read_text()
        self.assertIn(f'id="{u.compute_fp(new)}"', text)
        self.assertNotIn(f'id="{old}"', text)
        self.assertIn("Willkommen bei Atlas", text)
        self.assertIn('id="111"', text)
        shutil.rmtree(d)


if __name__ == "__main__":
    unittest.main(verbosity=2)
