#!/usr/bin/env python3
"""Naming-convention check for our C code (README "Code style"). Needs `pip install libclang`.

Parses every translation unit in the compile databases that exist
(test/host/build, test/hootl/build, build/ = production) and checks each declaration
that lives in our sources (main/, components/, test/host/, test/hootl/fake/*.c|h).
Names that are also declared by a vendor header (ESP-IDF, esp-zigbee, FreeRTOS, or the
HOOTL fake vendor headers in test/hootl/fake/include) are exempt: those implement the
vendor API. Also checks #define names and that fixed-width/float types use core.h.
Also:
  * every production TU is parsed a second time with a variant config (the bool Kconfig
    options in VARIANT_OFF undefined, EPD_ROTATION=3), so #if branches that are inactive in
    the default config get parsed and checked too;
  * whatever is still inactive (clang's skipped ranges) is checked lexically: each
    identifier there must be a known name (declared somewhere we parsed) or fit one of
    the conventions;
  * our .c/.h files that no compile DB covers are parsed with the flags of a sibling TU.
Exit 1 on any violation.

Usage: check_naming.py [--root DIR] [--db compile_commands.json ...] [--variant-all]
Negative tests: tools/test_check_naming.py"""
import glob, json, os, re, shlex, subprocess, sys
import clang.cindex as ci
from clang.cindex import CursorKind as CK, TypeKind as TK

import argparse, ctypes, tempfile

ROOT = OURS = FAKE_VENDOR = None
DBS = []
KEEP = {"app_main", "main"}
# '_'-prefixed names are reserved; only '__' names (compiler/libc) and these macro-hygiene
# temporaries (locals declared inside our own macro bodies) are exempt.
UNDERSCORE_ALLOW = {"_a", "_b", "_d", "_e"}   # tinytest.h CHECK_EQ, ssd1680.c CMD, zb_thermostat.c TRY
# Variant parse of the production TUs: these Kconfig bools off, plus the alternate panel rotation.
VARIANT_OFF = ("CONFIG_THERMO_DISPLAY_FAHRENHEIT", "CONFIG_THERMO_HW_WDT_ENABLE", "CONFIG_PM_ENABLE")
VARIANT_DEFS = ["-DEPD_ROTATION=3"]
C_KEYWORDS = set("""auto break case char const continue default do double else enum extern float for goto if
inline int long register restrict return short signed sizeof static struct switch typedef union unsigned
void volatile while _Bool _Static_assert _Alignas _Alignof _Noreturn _Generic _Atomic _Thread_local bool
true false NULL defined""".split())

def setup(root, dbs):
    global ROOT, OURS, FAKE_VENDOR, DBS
    ROOT = os.path.realpath(root)
    OURS = [ROOT + p for p in ("/main/", "/components/", "/test/host/", "/test/hootl/fake/")]
    FAKE_VENDOR = ROOT + "/test/hootl/fake/include/"
    DBS = dbs or [ROOT + "/test/host/build/compile_commands.json", ROOT + "/test/hootl/build/compile_commands.json",
                  ROOT + "/build/compile_commands.json"]

def ours(p):
    return bool(p) and not p.startswith(FAKE_VENDOR) and any(p.startswith(o) for o in OURS) \
        and "/build" not in p[len(ROOT):] and "/managed_components/" not in p

PASCAL = re.compile(r"^[A-Z][A-Za-z0-9]*$")
CAMEL = re.compile(r"^[a-z][A-Za-z0-9]*$")
PTR = re.compile(r"^p[A-Z0-9][A-Za-z0-9]*$")
KCONST = re.compile(r"^k[A-Z0-9][A-Za-z0-9]*$")
MACRO = re.compile(r"^[A-Z][A-Z0-9_]*$")

def rd(p):
    with open(p, errors="replace") as f: return f.read()

def wr(p, s):
    with open(p, "w") as f: f.write(s)

def is_ptr(t):
    return t.kind == TK.POINTER and t.get_pointee().get_canonical().kind not in (TK.FUNCTIONPROTO, TK.FUNCTIONNOPROTO)

def is_const(t):
    while t.kind in (TK.CONSTANTARRAY, TK.INCOMPLETEARRAY):
        t = t.element_type
    return t.is_const_qualified()

def expect(c):
    k, n = c.kind, c.spelling
    if k == CK.FUNCTION_DECL: return PASCAL, "PascalCase function"
    if k in (CK.TYPEDEF_DECL, CK.STRUCT_DECL, CK.UNION_DECL, CK.ENUM_DECL): return PASCAL, "PascalCase type"
    if k == CK.ENUM_CONSTANT_DECL: return KCONST, "kCamelCase enum value"
    if k == CK.VAR_DECL:
        filescope = c.semantic_parent is not None and c.semantic_parent.kind == CK.TRANSLATION_UNIT
        static = c.storage_class == ci.StorageClass.STATIC
        if (filescope or static) and is_const(c.type): return KCONST, "kCamelCase constant"
        if (filescope or static) and is_ptr(c.type) and c.type.get_pointee().is_const_qualified() and KCONST.match(n):
            return KCONST, "kCamelCase constant"   # e.g. static const char *kTag = "...": a constant string
        body = (PTR if is_ptr(c.type) else CAMEL).pattern[1:]
        if filescope or static:
            pre = "s_" if static else "g_"
            return re.compile("^" + pre + body), f"{pre}-prefixed {'pointer ' if is_ptr(c.type) else ''}variable"
        return (PTR if is_ptr(c.type) else CAMEL), "camelCase local" + (" pointer (pName)" if is_ptr(c.type) else "")
    if k in (CK.PARM_DECL, CK.FIELD_DECL):
        what = "parameter" if k == CK.PARM_DECL else "field"
        return (PTR if is_ptr(c.type) else CAMEL), f"camelCase {what}" + (" pointer (pName)" if is_ptr(c.type) else "")
    if k == CK.MACRO_DEFINITION: return MACRO, "UPPER_SNAKE macro"
    return None, None

DECLS = {CK.FUNCTION_DECL, CK.VAR_DECL, CK.PARM_DECL, CK.FIELD_DECL, CK.TYPEDEF_DECL, CK.STRUCT_DECL,
         CK.UNION_DECL, CK.ENUM_DECL, CK.ENUM_CONSTANT_DECL, CK.MACRO_DEFINITION}

def our_files():
    fs = glob.glob(ROOT + "/main/*.[ch]") + glob.glob(ROOT + "/components/**/*.[ch]", recursive=True) \
        + glob.glob(ROOT + "/test/host/*.[ch]") + glob.glob(ROOT + "/test/hootl/fake/*.[ch]")
    return sorted(os.path.realpath(f) for f in fs if ours(os.path.realpath(f)))

def clean_args(e, src):
    """Compile-DB entry -> libclang args (drop gcc-only/riscv flags, the source and -o)."""
    args, out, skip = shlex.split(e["command"])[1:] if "command" in e else e["arguments"][1:], [], False
    for a in args:
        if skip: skip = False; continue
        if a == "-o": skip = True; continue
        if a == "-c" or a.startswith(("-fsanitize", "-march", "-mabi", "-fstrict-volatile", "-fno-tree-switch",
                                      "-fno-shrink-wrap", "-nostartfiles", "-fmacro-prefix")) \
                or os.path.realpath(os.path.join(e["directory"], a)) == src:
            continue
        out.append(a)
    gcc = (shlex.split(e["command"]) if "command" in e else e["arguments"])[0]
    if "riscv" in os.path.basename(gcc):
        sysroot = subprocess.check_output([gcc, "-print-file-name=include"], text=True).strip()
        out = ["--target=riscv32-unknown-elf", "-isystem", sysroot,
               "-isystem", os.path.join(os.path.dirname(gcc), "..", "riscv32-esp-elf", "include")] + out
    else:
        out += ["-isystem", subprocess.check_output(["gcc", "-print-file-name=include"], text=True).strip()]
    return out, "riscv" in os.path.basename(gcc)

def variant_args(args, tmp):
    """Same args, but sdkconfig.h resolved from a copy with VARIANT_OFF removed, plus VARIANT_DEFS."""
    for k, a in enumerate(args):
        d = a[2:] if a.startswith("-I") and len(a) > 2 else (args[k + 1] if a == "-I" and k + 1 < len(args) else None)
        if d and os.path.exists(os.path.join(d, "sdkconfig.h")):
            out = os.path.join(tmp, "sdkconfig.h")
            if not os.path.exists(out):
                lines = rd(os.path.join(d, "sdkconfig.h")).splitlines()
                off = re.compile(r"#define (%s)\b" % "|".join(VARIANT_OFF))
                wr(out, "\n".join(l for l in lines if not off.match(l)) + "\n")
            return ["-I" + tmp] + args + VARIANT_DEFS
    return None

class _Range(ctypes.Structure):
    _fields_ = [("count", ctypes.c_uint), ("ranges", ctypes.POINTER(ci.SourceRange))]

def in_macro(c, f, n):
    """True if declaration c (named n) comes from a macro expansion: the source text at its
    (expansion) location is the macro's name, not n written out literally."""
    ls = rd(f).splitlines()
    ln, col = c.location.line, c.location.column
    return not (0 < ln <= len(ls) and ls[ln - 1][col - 1:col - 1 + len(n)] == n)

def skipped_ranges(tu):
    """[(file, startOffset, endOffset)] of #if branches clang did not parse."""
    lib = ci.conf.lib
    fn = lib.clang_getAllSkippedRanges
    fn.argtypes, fn.restype = [ci.TranslationUnit], ctypes.POINTER(_Range)
    rl = fn(tu)
    out = []
    if rl:
        for k in range(rl.contents.count):
            r = rl.contents.ranges[k]
            if r.start.file is not None:
                out.append((os.path.realpath(r.start.file.name), r.start.offset, r.end.offset))
        lib.clang_disposeSourceRangeList.argtypes = [ctypes.POINTER(_Range)]
        lib.clang_disposeSourceRangeList(rl)
    return out

DECL_KW = {"void", "int", "char", "short", "long", "unsigned", "signed", "bool", "struct", "union", "enum",
           "static", "extern", "inline", "const", "volatile"}
STRIP = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
ANY_OK = [PASCAL, CAMEL, PTR, KCONST, MACRO, re.compile(r"^[sg]_p?[a-zA-Z][A-Za-z0-9]*$")]

def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    ap.add_argument("--db", action="append", default=[])
    ap.add_argument("--variant-all", action="store_true", help="variant-parse every TU, not only production ones")
    a = ap.parse_args(argv)
    setup(a.root, a.db)
    found, vendor, known, idx, seen = {}, set(), set(), ci.Index.create(), set()
    covered, skipped = set(), []
    dbs = [d for d in DBS if os.path.exists(d)]
    if not dbs:
        sys.exit("no compile_commands.json: build test/host and test/hootl first")
    tmp = tempfile.mkdtemp(prefix="check_naming_")
    jobs, sibling = [], {}          # (src, args), dir -> args of a TU in that dir
    for db in dbs:
        for e in json.loads(rd(db)):
            src = os.path.realpath(os.path.join(e["directory"], e["file"]))
            if not ours(src) or (db, src) in seen: continue
            seen.add((db, src))
            args, prod = clean_args(e, src)
            jobs.append((src, args))
            sibling.setdefault(os.path.dirname(src), args)
            sibling.setdefault(None, args)
            if prod or a.variant_all:
                v = variant_args(args, tmp)
                if v: jobs.append((src, v))
    def parse(src, args, extra=()):
        tu = idx.parse(src, list(extra) + args, options=ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
        covered.add(src)
        for inc in tu.get_includes():
            covered.add(os.path.realpath(inc.include.name))
        skipped.extend(skipped_ranges(tu))
        for c in tu.cursor.walk_preorder():
            if c.kind not in DECLS or not c.spelling or c.location.file is None: continue
            known.add(c.spelling)
            f = os.path.realpath(c.location.file.name)
            usr = c.get_usr() or ("macro:" + c.spelling)
            if not ours(f):
                vendor.add(usr); continue
            n = c.spelling
            if n in KEEP or " " in n or n.startswith("__"): continue
            if n.startswith("_") and n in UNDERSCORE_ALLOW and c.kind == CK.VAR_DECL and in_macro(c, f, n): continue
            rx, what = expect(c)
            if n.startswith("_"):
                rx, what = re.compile("^$"), what + " (no leading '_': reserved name)"
            if rx and not rx.match(n):
                found.setdefault(usr, (f[len(ROOT) + 1:], c.location.line, n, what))
    for src, args in jobs:
        parse(src, args)
    nfiles, extra = len({j[0] for j in jobs}), 0
    # our files no compile DB reaches (not built in any config, or headers nobody includes)
    for f in our_files():
        if f in covered: continue
        args = sibling.get(os.path.dirname(f)) or sibling.get(os.path.dirname(os.path.dirname(f))) or sibling[None]
        parse(f, args + ["-I" + os.path.dirname(f)], ["-x", "c"] if f.endswith(".h") else [])
        extra += 1
    bad = sorted(v for u, v in found.items() if u not in vendor)
    # still-inactive #if branches: lexical check of each identifier
    text = {}
    lexbad = set()
    for f, s0, s1 in skipped:
        if not ours(f): continue
        if f not in text:
            text[f] = STRIP.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), rd(f))
        t = text[f]
        for m in re.finditer(r"\b[A-Za-z_]\w*\b", t[s0:s1]):
            n = m.group(0)
            line_start = t.rfind("\n", 0, s0 + m.start()) + 1
            if t[line_start:s0 + m.start()].lstrip().startswith("#"): continue   # directive lines
            # a call (name followed by '(' but not preceded by a type/declarator) is a use of a
            # name declared elsewhere, possibly by a vendor header that branch would include
            before = re.search(r"(\w+|\S)\s*$", t[max(0, s0 + m.start() - 80):s0 + m.start()])
            if re.match(r"\s*\(", t[s0 + m.end():s0 + m.end() + 40]) and \
                    not (before and re.match(r"\w|\*", before.group(1)) and before.group(1) not in C_KEYWORDS - DECL_KW):
                continue
            if n in C_KEYWORDS or n in known or n.startswith("__"): continue
            if not n.startswith("_") and any(r.match(n) for r in ANY_OK): continue
            lexbad.add((f[len(ROOT) + 1:], t.count("\n", 0, s0 + m.start()) + 1, n,
                        "a conventional name (unknown identifier in an inactive #if branch)"))
    bad += sorted(lexbad)
    # fixed-width / float types must come from core.h
    TYPES = re.compile(r"\b(u?int(8|16|32|64)_t|float|double)\b")
    for f in our_files():
        if f.endswith("/core/include/core.h"): continue
        s = STRIP.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), rd(f))
        for m in TYPES.finditer(s):
            bad.append((f[len(ROOT) + 1:], s.count("\n", 0, m.start()) + 1, m.group(0), "use core.h type (U8..U64, I8..I64, F32, F64)"))
    for f, line, name, what in bad:
        print(f"{f}:{line}: '{name}' should be {what}")
    print(f"check_naming: {len(bad)} violation(s); {len(jobs)} parses of {nfiles} compile-DB files "
          f"(incl. variant config), {extra} file(s) outside the compile DBs")
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
