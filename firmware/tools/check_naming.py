#!/usr/bin/env python3
"""Naming-convention check for our C code (README "Code style"). Needs `pip install libclang`.

Parses every translation unit in the compile databases that exist
(test/host/build, test/hootl/build, build/ = production) and checks each declaration
that lives in our sources (main/, components/, test/host/, test/hootl/fake/*.c|h).
Names that are also declared by a vendor header (ESP-IDF, esp-zigbee, FreeRTOS, or the
HOOTL fake vendor headers in test/hootl/fake/include) are exempt: those implement the
vendor API. Also checks #define names and that fixed-width/float types use core.h.
Exit 1 on any violation."""
import glob, json, os, re, shlex, subprocess, sys
import clang.cindex as ci
from clang.cindex import CursorKind as CK, TypeKind as TK

ROOT = os.path.realpath(os.path.join(os.path.dirname(__file__), ".."))
OURS = [ROOT + p for p in ("/main/", "/components/", "/test/host/", "/test/hootl/fake/")]
FAKE_VENDOR = ROOT + "/test/hootl/fake/include/"
KEEP = {"app_main", "main"}
DBS = [ROOT + "/test/host/build/compile_commands.json", ROOT + "/test/hootl/build/compile_commands.json",
       ROOT + "/build/compile_commands.json"]

def ours(p):
    return p and not p.startswith(FAKE_VENDOR) and any(p.startswith(o) for o in OURS) and "/build/" not in p[len(ROOT):]

PASCAL = re.compile(r"^[A-Z][A-Za-z0-9]*$")
CAMEL = re.compile(r"^[a-z][A-Za-z0-9]*$")
PTR = re.compile(r"^p[A-Z0-9][A-Za-z0-9]*$")
KCONST = re.compile(r"^k[A-Z0-9][A-Za-z0-9]*$")
MACRO = re.compile(r"^[A-Z][A-Z0-9_]*$")

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

def main():
    gccinc = subprocess.check_output(["gcc", "-print-file-name=include"], text=True).strip()
    found, vendor, idx, seen = {}, set(), ci.Index.create(), set()
    dbs = [d for d in DBS if os.path.exists(d)]
    if not dbs:
        sys.exit("no compile_commands.json: build test/host and test/hootl first")
    for db in dbs:
        for e in json.load(open(db)):
            src = os.path.realpath(os.path.join(e["directory"], e["file"]))
            if not ours(src) or (db, src) in seen: continue
            seen.add((db, src))
            args, out, skip = shlex.split(e["command"])[1:], [], False
            for a in args:
                if skip: skip = False; continue
                if a == "-o": skip = True; continue
                if a == "-c" or a.startswith(("-fsanitize", "-march", "-mabi", "-fstrict-volatile", "-fno-tree-switch",
                                              "-fno-shrink-wrap", "-nostartfiles", "-fmacro-prefix")) \
                        or os.path.realpath(os.path.join(e["directory"], a)) == src:
                    continue
                out.append(a)
            if db.startswith(ROOT + "/build/"):
                gcc = shlex.split(e["command"])[0]
                sysroot = subprocess.check_output([gcc, "-print-file-name=include"], text=True).strip()
                out = ["--target=riscv32-unknown-elf", "-isystem", sysroot,
                       "-isystem", os.path.join(os.path.dirname(gcc), "..", "riscv32-esp-elf", "include")] + out
            else:
                out += ["-isystem", gccinc]
            tu = idx.parse(src, out, options=ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
            for c in tu.cursor.walk_preorder():
                if c.kind not in DECLS or not c.spelling or c.location.file is None: continue
                f = os.path.realpath(c.location.file.name)
                usr = c.get_usr() or ("macro:" + c.spelling)
                if not ours(f):
                    vendor.add(usr); continue
                if c.spelling in KEEP or c.spelling.startswith("_") or " " in c.spelling: continue
                if c.kind == CK.MACRO_DEFINITION and f.endswith(".h") is False and False: pass
                rx, what = expect(c)
                if rx and not rx.match(c.spelling):
                    found.setdefault(usr, (f[len(ROOT) + 1:], c.location.line, c.spelling, what))
    bad = sorted(v for u, v in found.items() if u not in vendor)
    # fixed-width / float types must come from core.h
    TYPES = re.compile(r"\b(u?int(8|16|32|64)_t|float|double)\b")
    STRIP = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
    for f in sorted(glob.glob(ROOT + "/main/*.[ch]") + glob.glob(ROOT + "/components/**/*.[ch]", recursive=True)
                    + glob.glob(ROOT + "/test/host/*.[ch]") + glob.glob(ROOT + "/test/hootl/fake/*.[ch]")):
        if f.endswith("/core/include/core.h"): continue
        s = STRIP.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), open(f).read())
        for m in TYPES.finditer(s):
            bad.append((f[len(ROOT) + 1:], s.count("\n", 0, m.start()) + 1, m.group(0), "use core.h type (U8..U64, I8..I64, F32, F64)"))
    for f, line, name, what in bad:
        print(f"{f}:{line}: '{name}' should be {what}")
    print(f"check_naming: {len(bad)} violation(s) in {len(seen)} translation units")
    sys.exit(1 if bad else 0)

if __name__ == "__main__":
    main()
