#!/usr/bin/env python3
"""Negative/positive tests for tools/check_naming.py: builds a tiny fake tree with its own
compile_commands.json and checks exactly which names get flagged. Run: python tools/test_check_naming.py"""
import io, json, os, sys, tempfile, unittest, contextlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_naming
from check_naming import rd, wr

FILES = {
    "components/core/include/core.h": "#pragma once\n#include <stdint.h>\ntypedef uint8_t U8;\ntypedef int32_t I32;\n",
    "cfg/sdkconfig.h": "#define CONFIG_THERMO_HW_WDT_ENABLE 1\n",
    "main/good.h": "#pragma once\n#include \"core.h\"\n/* doc */\ntypedef struct { U8 fieldOne; U8 *pBuf; } GoodThing;\n"
                   "enum { kGoodValue = 1 };\n#define GOOD_MACRO 3\nvoid GoodFunc(GoodThing *pT);\n",
    "main/good.c": "#include \"good.h\"\n#include \"sdkconfig.h\"\nstatic int s_count;\nstatic U8 *s_pState;\nI32 g_total;\n"
                   "static const char *kTag = \"t\";\n"
                   "#define SWAP(x) do { int _a = (x); (void)_a; } while (0)\n"
                   "void GoodFunc(GoodThing *pT) { int localVal = 0; SWAP(localVal); (void)pT; (void)kTag; s_count++; (void)s_pState; }\n"
                   "#if CONFIG_THERMO_HW_WDT_ENABLE\nstatic void WdtOn(void) {}\n#else\nstatic void WdtOff(void) {}\n#endif\n",
    "main/bad.c": "#include \"core.h\"\n#include \"sdkconfig.h\"\n"
                  "static int bad_static;\n"                       # missing s_ / camel
                  "void bad_func(void) {}\n"                        # not Pascal
                  "static int _hiddenBad;\n"                        # leading _
                  "static void _pHidden(void) { (void)_hiddenBad; (void)bad_static; }\n"
                  "typedef struct { int Bad_field; } bad_type_t;\n"
                  "enum { BAD_ENUM = 1 };\n"
                  "#define badMacro 1\n"
                  "void TakesPtr(int *bad) { (void)bad; }\n"
                  "uint8_t RawType(void) { return 0; }\n"
                  "#if CONFIG_THERMO_HW_WDT_ENABLE\n#else\nstatic int bad_variant;\n#endif\n"   # only in variant parse
                  "#if 0\nstatic int bad_inactive;\n#endif\n",     # never active: lexical check
    "components/orphan/orphan.c": "void orphan_func(void) {}\n",   # in no compile DB
}
BAD = {"bad_static", "bad_func", "_hiddenBad", "_pHidden", "Bad_field", "bad_type_t", "BAD_ENUM", "badMacro",
       "bad", "uint8_t", "bad_variant", "bad_inactive", "orphan_func"}

class CheckNamingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        r = cls.tmp.name
        for p, body in FILES.items():
            os.makedirs(os.path.dirname(os.path.join(r, p)), exist_ok=True)
            wr(os.path.join(r, p), body)
        inc = f"-I{r}/components/core/include -I{r}/cfg -I{r}/main"
        db = [{"directory": r, "file": f"main/{n}", "command": f"gcc {inc} -c main/{n} -o {n}.o"} for n in ("good.c", "bad.c")]
        cls.db = os.path.join(r, "compile_commands.json")
        wr(cls.db, json.dumps(db))
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            cls.rc = check_naming.main(["--root", r, "--db", cls.db, "--variant-all"])
        cls.out = out.getvalue()
        cls.flagged = {l.split("'")[1] for l in cls.out.splitlines() if "should be" in l}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_exit_code(self):
        self.assertEqual(self.rc, 1, self.out)

    def test_every_bad_name_flagged(self):
        self.assertEqual(BAD - self.flagged, set(), self.out)

    def test_no_false_positives(self):
        self.assertEqual(self.flagged - BAD, set(), self.out)

    def test_good_file_alone_passes(self):
        r = self.tmp.name
        db = os.path.join(r, "good_only.json")
        wr(db, json.dumps([e for e in json.loads(rd(self.db)) if e["file"].endswith("good.c")]))
        os.rename(f"{r}/main/bad.c", f"{r}/bad.c.off"); os.rename(f"{r}/components/orphan", f"{r}/orphan.off")
        try:
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                rc = check_naming.main(["--root", r, "--db", db, "--variant-all"])
            self.assertEqual(rc, 0, out.getvalue())
        finally:
            os.rename(f"{r}/bad.c.off", f"{r}/main/bad.c"); os.rename(f"{r}/orphan.off", f"{r}/components/orphan")

if __name__ == "__main__":
    unittest.main()
