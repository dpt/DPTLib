#!/usr/bin/env python3
"""release.py -- cut, merge, tag, start and push releases of DPTLib and Wuss.

Run from the repository root. The flow, one step per command:

  release.py check          -- read-only preflight for a cut
  release.py cut            -- date the CHANGELOG (and SUMMARY) and commit
  release.py merge          -- merge develop into main, sign the tag, back to develop
  release.py start [X.Y.Z]  -- set the next version (default: next minor) and commit
  release.py push           -- push develop, main and the current version's tag

The version is the one in CMakeLists.txt, set by the previous `start`. Add
`--dry-run` to print what would change, `--test` to run the test suite before
a cut and `--trailer "Key: value"` to add a commit trailer.
"""

import argparse
import datetime
import os
import re
import subprocess
import sys

# How each repository merges develop into main. DPTLib keeps a linear main;
# Wuss records a merge commit for each release.
MERGE_STYLE = {"DPTLib": "--ff-only", "Wuss": "--no-ff"}

PROJECT_RE = re.compile(r"^(project\((\w+) VERSION )(\d+\.\d+\.\d+)", re.M)
TARGET_RE = re.compile(r"^(    VERSION )(\d+\.\d+\.\d+)$", re.M)
README_RE = re.compile(r"^(version )(\d+\.\d+\.\d+)$", re.M)
PIN_RE = re.compile(r"FetchContent_Declare\(dptlib\s.*?GIT_TAG (\S+?)\)", re.S)
SUMMARY_RE = re.compile(r"^Unreleased\. (Changes since .*?)(?=\n\n)", re.M | re.S)
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")

args = None


def die(msg):
    sys.exit("release: " + msg)


def git(*argv, check=True):
    """Run git and return its stripped stdout."""
    r = subprocess.run(["git", *argv], capture_output=True, text=True)
    if check and r.returncode != 0:
        die("git %s failed:\n%s" % (" ".join(argv), r.stderr.strip()))
    return r.stdout.strip()


def act(*argv, timeout=None):
    """Run a command that changes state, or print it under --dry-run.
    stdin is closed so a signing prompt fails rather than hangs."""
    print("+ " + " ".join(argv))
    if args.dry_run:
        return
    try:
        r = subprocess.run(argv, stdin=subprocess.DEVNULL, timeout=timeout)
    except subprocess.TimeoutExpired:
        die("%s timed out: is the signing key loaded (ssh-add)?" % argv[1])
    if r.returncode != 0:
        die("%s failed" % " ".join(argv))


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def write(path, text):
    print("  edit " + path)
    if not args.dry_run:
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)


def project():
    """Return (name, version) from CMakeLists.txt."""
    m = PROJECT_RE.search(read("CMakeLists.txt"))
    if not m:
        die("no project(... VERSION ...) in CMakeLists.txt: run from a repository root")
    return m.group(2), m.group(3)


def commit(subject, body, paths):
    argv = ["git", "commit", "-m", subject]
    if body:
        argv += ["-m", body]
    for t in args.trailer:
        argv += ["--trailer", t]
    act("git", "add", "--", *paths)
    act(*argv, "--", *paths, timeout=30)


def require_clean_develop():
    if git("rev-parse", "--abbrev-ref", "HEAD") != "develop":
        die("not on develop")
    if git("status", "--porcelain", "--untracked-files=no"):
        die("tracked files have uncommitted changes")


# ----------------------------------------------------------------------------


def preflight():
    """Return the list of problems that block a cut."""
    problems = []
    name, version = project()
    cmake = read("CMakeLists.txt")
    readme = README_RE.search(read("README.md"))
    log = read("CHANGELOG.md")

    targets = TARGET_RE.findall(cmake)
    if not targets or any(v != version for _, v in targets):
        problems.append("CMakeLists.txt target VERSION does not match %s" % version)
    if not readme or readme.group(2) != version:
        problems.append("README.md version does not match %s" % version)
    if git("tag", "-l", version):
        problems.append("tag %s exists: run `start` first" % version)
    if "## [%s]" % version in log:
        problems.append("CHANGELOG.md already has a %s section" % version)
    unreleased = re.search(r"^## \[Unreleased\]\n(.*?)(?=^## \[|\Z)", log, re.M | re.S)
    if not unreleased:
        problems.append("CHANGELOG.md has no ## [Unreleased] section")
    elif not unreleased.group(1).strip():
        problems.append("CHANGELOG.md [Unreleased] section is empty")

    if os.path.exists("SUMMARY.md"):
        summary = read("SUMMARY.md")
        if not summary.startswith("# %s %s\n" % (name, version)):
            problems.append("SUMMARY.md heading is not '# %s %s'" % (name, version))
        if not SUMMARY_RE.search(summary):
            problems.append("SUMMARY.md has no 'Unreleased. Changes since ...' line")

    pin = PIN_RE.search(cmake)
    if pin:
        tag = pin.group(1)
        if not VERSION_RE.match(tag):
            problems.append("DPTLib GIT_TAG is '%s', not a release tag" % tag)
        elif os.path.isdir("../DPTLib/.git") and not git("-C", "../DPTLib", "tag", "-l", tag):
            problems.append("DPTLib GIT_TAG %s is not a tag in ../DPTLib" % tag)

    git("fetch", "--quiet", "origin")
    for branch in ("develop", "main"):
        behind = git("rev-list", "--count", "%s..origin/%s" % (branch, branch))
        if behind != "0":
            problems.append("%s is %s commits behind origin/%s" % (branch, behind, branch))
    # Release merges sit only on main, so ignore merge commits.
    if git("rev-list", "--no-merges", "main", "^develop"):
        problems.append("main has commits that develop lacks")
    return problems


def run_tests(name):
    test = "%sTest" % name
    act("cmake", "--build", "build-asan", "--target", test)
    act("./build-asan/%s-debug" % test, "-resources", ".")


def cmd_check():
    require_clean_develop()
    problems = preflight()
    for p in problems:
        print("  - " + p)
    if problems:
        die("%d problem(s) block the cut" % len(problems))
    print("ready to cut %s %s" % project())


def cmd_cut():
    require_clean_develop()
    problems = preflight()
    if problems:
        die("cannot cut:\n  - " + "\n  - ".join(problems))
    name, version = project()
    if args.test:
        run_tests(name)
    today = datetime.date.today().isoformat()
    paths = ["CHANGELOG.md"]

    log = read("CHANGELOG.md")
    write("CHANGELOG.md", log.replace("## [Unreleased]\n",
                                      "## [Unreleased]\n\n## [%s] - %s\n" % (version, today), 1))
    body = "Move the unreleased changes into a %s section." % version

    if os.path.exists("SUMMARY.md"):
        summary = read("SUMMARY.md")
        line = "Released %s. %s" % (today, " ".join(SUMMARY_RE.search(summary).group(1).split()))
        write("SUMMARY.md", SUMMARY_RE.sub(wrap(line), summary, 1))
        paths.append("SUMMARY.md")
        body = "Move the unreleased changes into a %s section and mark the summary released." % version

    commit("chore(release): cut %s" % version, body, paths)


def wrap(text, width=76):
    """Wrap a paragraph the way SUMMARY.md is wrapped."""
    lines, line = [], ""
    for word in text.split():
        if line and len(line) + 1 + len(word) > width:
            lines.append(line)
            line = word
        else:
            line = (line + " " + word) if line else word
    lines.append(line)
    return "\n".join(lines).replace("\\", "\\\\")


def cmd_merge():
    require_clean_develop()
    name, version = project()
    if git("tag", "-l", version):
        die("tag %s exists" % version)
    if git("log", "-1", "--format=%s") != "chore(release): cut %s" % version:
        die("develop's tip is not the %s cut" % version)
    style = MERGE_STYLE.get(name)
    if not style:
        die("no merge style for %s in MERGE_STYLE" % name)
    act("git", "checkout", "main")
    if style == "--no-ff":
        act("git", "merge", "--no-ff", "develop", "-m", "Merge branch 'develop'", timeout=30)
    else:
        act("git", "merge", "--ff-only", "develop")
    act("git", "tag", "-s", version, "-m", version, timeout=30)
    act("git", "checkout", "develop")


def cmd_start():
    require_clean_develop()
    name, version = project()
    if not git("tag", "-l", version):
        die("%s is not tagged yet: cut and merge it first" % version)
    if args.version:
        new = args.version
        if not VERSION_RE.match(new):
            die("version must be X.Y.Z")
    else:
        major, minor, _ = version.split(".")
        new = "%s.%d.0" % (major, int(minor) + 1)

    cmake = read("CMakeLists.txt")
    cmake = PROJECT_RE.sub(lambda m: m.group(1) + new, cmake, 1)
    cmake = TARGET_RE.sub(lambda m: m.group(1) + new, cmake)
    write("CMakeLists.txt", cmake)
    write("README.md", README_RE.sub(lambda m: m.group(1) + new, read("README.md"), 1))
    paths = ["CMakeLists.txt", "README.md"]

    if os.path.exists("SUMMARY.md"):
        print("  note: SUMMARY.md still describes %s; rewrite it as '# %s %s' with an"
              " 'Unreleased. Changes since %s; ...' line before the next cut"
              % (version, name, new, version))

    commit("chore: start %s" % new, None, paths)


def cmd_push():
    _, version = project()
    tag = git("describe", "--tags", "--abbrev=0", "main")
    if not VERSION_RE.match(tag):
        die("main has no release tag")
    print("  pushing tag %s (CMake is at %s)" % (tag, version))
    os.environ["GIT_SSH_COMMAND"] = "ssh -o BatchMode=yes"
    act("git", "push", "origin", "develop", "main", tag, timeout=120)


def main():
    global args
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("command", choices=["check", "cut", "merge", "start", "push"])
    p.add_argument("version", nargs="?", help="for start: the next version")
    p.add_argument("-n", "--dry-run", action="store_true", help="change nothing")
    p.add_argument("--test", action="store_true", help="cut: build and run the tests first")
    p.add_argument("--trailer", action="append", default=[],
                   help="add a commit trailer, e.g. 'Co-Authored-By: ...'")
    args = p.parse_args()
    if args.version and args.command != "start":
        p.error("only start takes a version")
    globals()["cmd_" + args.command]()


if __name__ == "__main__":
    main()
