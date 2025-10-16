#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
dupdef_scan_cpp.py — detect duplicate C++ definitions (classes/functions/methods).

Features
--------
- Walks source tree; scans C/C++ headers/impl files.
- Strips comments and string/char literals safely.
- Finds:
  1) Duplicate class/struct/enum/union *definitions* (same fully-qualified name).
  2) Duplicate free functions and member functions *definitions* (same FQN + normalized signature).
  3) Flags identical-duplicate bodies vs. conflicting bodies (ODR risk).
- JSON or text output; CI-friendly nonzero exit with --fail-on-issues.

Heuristics
----------
- Lightweight parser (no libclang needed).
- Namespaces & nested classes tracked via a simple brace/namespace stack.
- Function signature normalization removes parameter names & defaults.
- Recognizes cv-qualifiers (const), ref-qualifiers (&, &&), noexcept, trailing return types.
- Ignores *declarations* (ends with ';'); only flags *definitions* (has '{...}').

Limitations
-----------
- It's a robust heuristic, not a full C++ parser. Works well for most codebases.
- Overloads: different normalized parameter types are *not* duplicates (OK).
- Inline/template functions: allowed across headers if body **identical** (configurable).

Usage
-----
  python dupdef_scan_cpp.py [paths...] \
      --exclude third_party --exclude build \
      --json-out dup_report.json --fail-on-issues

"""

from __future__ import annotations
import argparse, json, os, re, sys, hashlib, bisect
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Iterable

CPP_EXTS = {".h", ".hh", ".hpp", ".hxx", ".ipp",
            ".c", ".cc", ".cpp", ".cxx", ".cu", ".cuh"}

# ------------------ Utilities ------------------

def iter_files(paths: List[Path], exts=CPP_EXTS, excludes: List[str]=[]) -> Iterable[Path]:
    globs = [re.compile(fnmatch_to_re(pat)) for pat in excludes]
    for root in paths:
        if root.is_file():
            if root.suffix.lower() in exts and not any(g.search(str(root)) for g in globs):
                yield root
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            full_dir = Path(dirpath)
            # skip excluded directories quickly
            if any(g.search(str(full_dir)) for g in globs):
                dirnames[:] = []  # don't descend
                continue
            for fn in filenames:
                p = full_dir / fn
                if p.suffix.lower() in exts and not any(g.search(str(p)) for g in globs):
                    yield p

def fnmatch_to_re(pat: str) -> str:
    # crude glob→regex (supports '*' and '**')
    pat = pat.replace(".", r"\.").replace("+", r"\+")
    pat = pat.replace("**/", r".*(/|^)").replace("**", r".*")
    pat = pat.replace("*", r"[^/]*").replace("?", r".")
    return r"^" + pat + r"$"

def sha1(s: str) -> str:
    return hashlib.sha1(s.encode("utf-8", "ignore")).hexdigest()

# ------------------ C++ preprocessor: remove comments / strings ------------------

def strip_comments_and_strings(src: str) -> str:
    """
    Remove //... and /*...*/ and string/char literals while preserving newlines/positions.
    """
    out = []
    i, n = 0, len(src)
    NORMAL, SLASH, LINE, BLOCK, STR, CHAR = range(6)
    state = NORMAL
    quote = ""
    while i < n:
        c = src[i]
        if state == NORMAL:
            if c == '/':
                state = SLASH
                i += 1
                continue
            elif c == '"':
                state = STR; quote = '"'; out.append('"'); i += 1; continue
            elif c == "'":
                state = CHAR; quote = "'"; out.append("'"); i += 1; continue
            else:
                out.append(c); i += 1; continue

        elif state == SLASH:
            if i < n and src[i] == '/':
                state = LINE; out.append(' '); i += 1; continue
            elif i < n and src[i] == '*':
                state = BLOCK; out.append(' '); i += 1; continue
            else:
                # **Fix:** not a comment — emit the prior '/' and reprocess current char in NORMAL.
                out.append('/')
                state = NORMAL
                continue

        elif state == LINE:
            if c == '\n':
                out.append('\n'); state = NORMAL
            else:
                out.append(' ')
            i += 1; continue

        elif state == BLOCK:
            if c == '*' and i+1 < n and src[i+1] == '/':
                out.append('  '); i += 2; state = NORMAL; continue
            out.append(' ' if c != '\n' else '\n'); i += 1; continue

        elif state in (STR, CHAR):
            if c == '\\':
                out.append('\\'); i += 1
                if i < n: out.append(' '); i += 1; continue
            out.append(quote if c == quote else ' ')
            if c == quote: state = NORMAL
            i += 1; continue

    return ''.join(out)

# ------------------ Lightweight C++ scanner ------------------

_id = r"[A-Za-z_]\w*"
ws = r"[ \t\r\n]*"

@dataclass
class ClassDef:
    fqname: str
    file: str
    line: int

@dataclass
class FuncDef:
    fqname: str
    params_norm: str  # normalized param types + cv/ref/noexcept
    file: str
    line: int
    body_hash: str
    is_inline_or_tpl: bool = False

@dataclass
class Findings:
    class_defs: Dict[str, List[ClassDef]] = field(default_factory=dict)
    func_defs: Dict[Tuple[str, str], List[FuncDef]] = field(default_factory=dict)  # (fqname, sig)->defs

    def add_class(self, c: ClassDef):
        self.class_defs.setdefault(c.fqname, []).append(c)

    def add_func(self, f: FuncDef):
        key = (f.fqname, f.params_norm)
        self.func_defs.setdefault(key, []).append(f)

def scan_cpp(text: str, fname: str) -> Findings:
    """
    Scan C++ source without full parse:
    - Tracks namespace stack.
    - Finds class/struct/enum/union names followed by '{' (definition).
    - Finds function/method definitions by header (...) { ... } and normalizes args.
    """
    stripped = strip_comments_and_strings(text)
    find = Findings()
    n = len(stripped)
    i = 0

    # Fast line number lookup
    nl_pos = [i for i, ch in enumerate(stripped) if ch == '\n']
    def line_of(pos: int) -> int:
        return bisect.bisect_right(nl_pos, pos) + 1

    ns_stack: List[str] = []
    class_stack: List[str] = []

    def skip_ws(k):
        while k < n and stripped[k] in " \t\r\n":
            k += 1
        return k

    def match_kw(k, kw):
        k = skip_ws(k)
        if stripped.startswith(kw, k) and (k+len(kw)==n or not stripped[k+len(kw)].isalnum() and stripped[k+len(kw)]!='_'):
            return k+len(kw)
        return -1

    def peek_ident_left(k):
        """backtrack from k (exclusive) to extract an identifier or X::Y qualified name"""
        j = k-1
        # skip spaces
        while j >= 0 and stripped[j].isspace(): j -= 1
        # now parse tokens backwards to assemble something like A::B::C
        tokens = []
        cur = []
        while j >= 0:
            ch = stripped[j]
            if ch.isalnum() or ch=='_' or ch in ['~', '>']:
                cur.append(ch); j -= 1; continue
            if ch == ':':
                # expect '::'
                if j-1 >= 0 and stripped[j-1]==':':
                    # finish current ident
                    ident = ''.join(reversed(cur)).strip()
                    if ident:
                        tokens.append(ident)
                    tokens.append('::')
                    cur = []
                    j -= 2
                    continue
                else:
                    break
            elif ch in " \t\r\n*&<>,":
                # end of ident piece
                if cur:
                    ident = ''.join(reversed(cur)).strip()
                    if ident:
                        tokens.append(ident)
                        cur=[]
                j -= 1
                # keep skipping qualifiers
                continue
            else:
                break
        if cur:
            tokens.append(''.join(reversed(cur)).strip())
        # tokens like ['Namespace', '::', 'Class', '::', 'func']
        tokens = list(reversed(tokens))
        # Clean consecutive '::'
        out = []
        for t in tokens:
            if t == '' or t == ',':
                continue
            out.append(t)
        name = ''.join(out).strip()
        return name

    def parse_balanced(k, open_ch='(', close_ch=')'):
        """ return (end_index_after_closer, content_inside) or (-1, '') """
        if k >= n or stripped[k] != open_ch:
            return -1, ''
        depth = 0
        j = k
        buf = []
        while j < n:
            ch = stripped[j]
            if ch == open_ch:
                depth += 1
            elif ch == close_ch:
                depth -= 1
                if depth == 0:
                    return j+1, ''.join(buf)
            buf.append(ch)
            j += 1
        return -1, ''

    def normalize_params(params: str, tail: str) -> str:
        # remove newline/extra spaces
        s = ' '.join(params.replace('\n',' ').replace('\r',' ').split())
        # drop default values
        s = re.sub(r"=\s*[^,)\[]+", "", s)
        # drop parameter names (heuristic: trailing identifier)
        parts = []
        depth = 0
        cur = []
        for ch in s:
            if ch == '<': depth += 1
            elif ch == '>': depth = max(0, depth-1)
            if ch == ',' and depth==0:
                parts.append(''.join(cur).strip())
                cur = []
            else:
                cur.append(ch)
        if cur: parts.append(''.join(cur).strip())
        norm_parts = []
        for p in parts:
            # remove trailing names (identifier possibly with [] or ref qualifiers)
            p = re.sub(r"\b([A-Za-z_]\w*)\s*(\[\s*\])*$", "", p).strip()
            p = re.sub(r"\s+", " ", p)
            # remove 'register'/'volatile' noise (keep const)
            p = re.sub(r"\b(register|volatile)\b", "", p).strip()
            norm_parts.append(p)
        args = ','.join(norm_parts)
        # tail qualifiers: const/noexcept/ref-qualifiers/-> trailing
        tail = tail.strip()
        # normalize spaces
        tail = ' '.join(tail.split())
        return args + ("|" + tail if tail else "")

    while i < n:
        # detect namespace blocks: namespace X { ... }
        j = skip_ws(i)
        if stripped.startswith("namespace", j):
            k = j + len("namespace")
            k = skip_ws(k)
            # anonymous namespace or named
            m = re.match(rf"{_id}", stripped[k:])
            if m:
                ns = m.group(0)
                k += len(ns)
            else:
                ns = ""  # anonymous
            k = skip_ws(k)
            if k < n and stripped[k] == '{':
                ns_stack.append(ns)
                i = k + 1
                continue

        # detect closing brace for namespace/class scopes to drop stacks
        if stripped[i] == '}':
            # pop class if needed (approximate: pop when we see '};' after class)
            # we don't strictly track braces per class; OK for duplication detection.
            if class_stack:
                class_stack.pop()
            if ns_stack:
                # only pop namespace if the previous open was a namespace (heuristic)
                # we can't easily distinguish; leave ns_stack pop conservative:
                ns_stack.pop()
            i += 1
            continue

        # class/struct/enum/union definitions
        for kw in ("class", "struct", "union", "enum class", "enum"):
            if stripped.startswith(kw, j) and re.match(r"\b", stripped[j+len(kw):]):
                k = j + len(kw)
                k = skip_ws(k)
                m = re.match(rf"{_id}", stripped[k:])
                if not m:
                    break
                name = m.group(0)
                k += len(name)
                # must be a definition if a '{' is ahead before ';'
                ahead = stripped[k:k+200]
                brace_pos = ahead.find('{')
                semi_pos  = ahead.find(';')
                if brace_pos != -1 and (semi_pos == -1 or brace_pos < semi_pos):
                    # capture FQN
                    fqn = '::'.join([n for n in ns_stack if n])  # ignore anonymous
                    if class_stack:
                        fqn = (fqn + ("::" if fqn else "") + "::".join(class_stack) + "::" + name) if fqn else "::".join(class_stack) + "::" + name
                    else:
                        fqn = (fqn + ("::" if fqn else "") + name) if fqn else name
                    line = line_of(j)
                    find.add_class(ClassDef(fqname=fqn, file=str(fname), line=line))
                    # push to class stack (best-effort)
                    class_stack.append(name)
                    i = j + 1
                    break
        # function/method definitions: look for (...) tail { ... }
        # Approach: find '(', parse to ')', then peek name before '(' and check body starts with '{'
        if stripped[i] == '(':
            # find header start: go back to name
            name = peek_ident_left(i)
            # skip false positives like if/for/switch/catch
            if name and not re.search(r"(?:^|::)(if|for|while|switch|catch|return)$", name):
                close_idx, inside = parse_balanced(i, '(', ')')
                if close_idx != -1:
                    # capture tail qualifiers + next token
                    k = skip_ws(close_idx)
                    tail_start = k
                    # consume possible 'const', 'noexcept', '&', '&&', trailing return
                    # don't consume '{' here
                    # trailing return '-> T'
                    # greedy but bounded
                    # collect until we hit '{' or ';'
                    while k < n and stripped[k] not in '{;':
                        k += 1
                    tail = stripped[tail_start:k]
                    # definition requires '{'
                    if k < n and stripped[k] == '{':
                        # Build FQN: include namespaces; for member methods prefixed with Class::method
                        # If name already qualified (contains '::'), use as-is with namespaces prefix only if name doesn't start with '::'
                        fqn = name
                        ns_prefix = '::'.join([n for n in ns_stack if n])
                        if '::' not in fqn.split('::')[0] and ns_prefix:
                            fqn = ns_prefix + "::" + fqn
                        params_norm = normalize_params(inside, tail)
                        # find body end brace
                        body_end = find_matching_brace(stripped, k)
                        body = stripped[k:body_end] if body_end != -1 else stripped[k:k+200]
                        body_hash = sha1(body)
                        # rough inline/template detection: preceding tokens include 'inline' or 'template<...>'
                        prefix = stripped[max(0, i-200):i]
                        is_inline = bool(re.search(r"\binline\b", prefix))
                        is_tpl = bool(re.search(r"\btemplate\s*<", prefix))
                        line = line_of(i)
                        find.add_func(FuncDef(fqname=fqn, params_norm=params_norm, file=str(fname),
                                              line=line, body_hash=body_hash,
                                              is_inline_or_tpl=(is_inline or is_tpl)))
                        i = k + 1
                        continue
            i += 1
            continue

        i += 1

    return find

def find_matching_brace(s: str, open_idx: int) -> int:
    """ given index of '{', return index after matching '}', ignoring braces in strings/comments (input already stripped). """
    if open_idx >= len(s) or s[open_idx] != '{': return -1
    depth = 0
    i = open_idx
    while i < len(s):
        ch = s[i]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return -1

# ------------------ Report building ------------------

def merge_findings(allf: List[Findings]):
    classes: Dict[str, List[ClassDef]] = {}
    funcs: Dict[Tuple[str,str], List[FuncDef]] = {}
    for f in allf:
        for k, v in f.class_defs.items():
            classes.setdefault(k, []).extend(v)
        for k, v in f.func_defs.items():
            funcs.setdefault(k, []).extend(v)
    return classes, funcs

def build_report(classes, funcs, allow_identical_inline=True):
    duplicate_classes = []
    for fqname, defs in classes.items():
        # duplicate if defined in multiple *files*
        files = {d.file for d in defs}
        if len(files) > 1:
            duplicate_classes.append({
                "fqname": fqname,
                "defs": [{"file": d.file, "line": d.line} for d in defs]
            })

    duplicate_functions = []
    odr_conflicts = []
    for (fqname, sig), defs in funcs.items():
        if len(defs) <= 1: continue
        # group by body hash
        by_hash: Dict[str, List[FuncDef]] = {}
        for d in defs:
            by_hash.setdefault(d.body_hash, []).append(d)
        if len(by_hash) == 1:
            # identical bodies across files
            if allow_identical_inline:
                # only flag if defined in multiple DIFFERENT files and none are explicitly inline/template?
                if any(not d.is_inline_or_tpl for d in defs):
                    duplicate_functions.append({
                        "fqname": fqname, "signature": sig,
                        "kind": "identical_noninline",
                        "defs": [{"file": d.file, "line": d.line} for d in defs]
                    })
            else:
                duplicate_functions.append({
                    "fqname": fqname, "signature": sig,
                    "kind": "identical",
                    "defs": [{"file": d.file, "line": d.line} for d in defs]
                })
        else:
            # conflicting bodies — ODR violation
            odr_conflicts.append({
                "fqname": fqname, "signature": sig,
                "variants": [
                    {"body_hash": h, "defs": [{"file": d.file, "line": d.line} for d in lst]}
                    for h, lst in by_hash.items()
                ]
            })

    return {
        "duplicate_classes": duplicate_classes,
        "duplicate_functions": duplicate_functions,
        "odr_conflicts": odr_conflicts,
    }

def print_report_text(report):
    out = []
    if report["duplicate_classes"]:
        out.append("== Duplicate class/struct/enum definitions ==")
        for item in report["duplicate_classes"]:
            out.append(f"  {item['fqname']}")
            for d in item["defs"]:
                out.append(f"    - {d['file']}:{d['line']}")
    if report["duplicate_functions"]:
        out.append("== Duplicate function/method definitions (identical bodies) ==")
        for item in report["duplicate_functions"]:
            out.append(f"  {item['fqname']}({item['signature']}) [{item.get('kind','identical')}]")
            for d in item["defs"]:
                out.append(f"    - {d['file']}:{d['line']}")
    if report["odr_conflicts"]:
        out.append("== Conflicting function/method definitions (ODR risk) ==")
        for item in report["odr_conflicts"]:
            out.append(f"  {item['fqname']}({item['signature']})")
            for var in item["variants"]:
                out.append(f"    body {var['body_hash'][:12]}:")
                for d in var["defs"]:
                    out.append(f"      - {d['file']}:{d['line']}")
    if not out:
        out.append("No duplicate C++ definitions found.")
    return "\n".join(out) + "\n"

# ------------------ CLI ------------------

def parse_args(argv=None):
    ap = argparse.ArgumentParser(description="Scan C++ codebase for duplicate definitions.")
    ap.add_argument("paths", nargs="*", default=["."], help="Files or directories to scan.")
    ap.add_argument("--exclude", action="append", default=[],
                    help="Glob/regex to exclude (e.g. 'build/**', 'third_party/**').")
    ap.add_argument("--json-out", default=None, help="Write JSON report to file.")
    ap.add_argument("--allow-identical-inline", action="store_true", default=True,
                    help="Allow identical inline/template function bodies across headers (default).")
    ap.add_argument("--no-allow-identical-inline", dest="allow_identical_inline",
                    action="store_false", help="Flag identical inline/template duplicates too.")
    ap.add_argument("--fail-on-issues", action="store_true", help="Exit 2 if any issues found.")
    ap.add_argument("--max-file-size-mb", type=int, default=5, help="Skip files bigger than this.")
    ap.add_argument("--jobs", type=int, default=0,
                    help="Number of parallel processes for scanning (0 = auto, 1 = no parallel).")
    return ap.parse_args(argv)

def scan_one_file(path: str, max_mb: int):
    p = Path(path)
    if p.stat().st_size > max_mb * 1024 * 1024:
        return None
    try:
        text = p.read_text(encoding="utf-8", errors="ignore")
    except Exception as e:
        return ("warn", f"[WARN] Could not read {p}: {e}")
    f = scan_cpp(text, str(p))
    return ("ok", f)

def main(argv=None):
    args = parse_args(argv)
    roots = [Path(p).resolve() for p in args.paths]
    files = list(iter_files(roots, exts=CPP_EXTS, excludes=args.exclude))
    all_findings: List[Findings] = []

    jobs = (os.cpu_count() or 2) if args.jobs == 0 else max(1, args.jobs)
    if jobs <= 1:
        for f in files:
            res = scan_one_file(str(f), args.max_file_size_mb)
            if res is None: continue
            kind, payload = res
            if kind == "warn": print(payload, file=sys.stderr); continue
            all_findings.append(payload)
    else:
        with ProcessPoolExecutor(max_workers=jobs) as ex:
            futs = {ex.submit(scan_one_file, str(f), args.max_file_size_mb): f for f in files}
            for fut in as_completed(futs):
                res = fut.result()
                if res is None: continue
                kind, payload = res
                if kind == "warn": print(payload, file=sys.stderr); continue
                all_findings.append(payload)

    classes, funcs = merge_findings(all_findings)
    report = build_report(classes, funcs, allow_identical_inline=args.allow_identical_inline)

    out_text = print_report_text(report)
    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as fp:
            json.dump(report, fp, indent=2)
        sys.stdout.write(out_text)
    else:
        sys.stdout.write(out_text)

    if args.fail_on_issues:
        has_issues = bool(report["duplicate_classes"] or report["duplicate_functions"] or report["odr_conflicts"])
        raise SystemExit(2 if has_issues else 0)

if __name__ == "__main__":
    main()
