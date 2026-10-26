#!/usr/bin/env python3
"""Exercise storage relocation using the actual runtime path initializer."""
import os
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="desert-storm-paths-") as temporary:
    base = Path(temporary)
    install, unrelated = base / "console folder", base / "other cwd"
    install.mkdir()
    unrelated.mkdir()
    harness = base / "check.c"
    harness.write_text(r'''
#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    if (ds_paths_init()) { perror("paths"); return 1; }
    const char *keys[] = {"DS_ROOT", "HOME", "DS_SAVE_DIR", "XDG_CONFIG_HOME",
        "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME", "TMPDIR",
        "DS_PAD_CONFIG", "XDG_RUNTIME_DIR"};
    for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; ++i)
        printf("%s=%s\n", keys[i], getenv(keys[i]));
    FILE *f = fopen("options.cfg", "a");
    if (!f) return 2;
    fputs("checked\n", f);
    if (fclose(f)) return 3;
    f = fopen("saves/test.sav", "w");
    if (!f) return 4;
    fputs("saved\n", f);
    return fclose(f) != 0;
}
''')
    executable = install / "check"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(repo / "runtime"), str(harness),
                    str(repo / "runtime/paths.c"), "-o", str(executable)], check=True)
    env = dict(os.environ, HOME=str(unrelated), XDG_CONFIG_HOME=str(unrelated),
               DS_PAD_CONFIG=str(unrelated / "wrong.cfg"), XDG_RUNTIME_DIR="/run")

    def run(program, root):
        result = subprocess.run([str(program)], cwd=unrelated, env=env,
                                text=True, capture_output=True, check=True)
        values = dict(line.split("=", 1) for line in result.stdout.splitlines())
        assert values == {
            "DS_ROOT": str(root), "HOME": str(root), "DS_SAVE_DIR": str(root / "saves"),
            "XDG_CONFIG_HOME": str(root / "config"), "XDG_DATA_HOME": str(root / "userdata"),
            "XDG_STATE_HOME": str(root / "userdata"), "XDG_CACHE_HOME": str(root / "cache"),
            "TMPDIR": str(root / "tmp"), "DS_PAD_CONFIG": str(root / "controller-buttons.cfg"),
            "XDG_RUNTIME_DIR": "/run",
        }, values
        assert (root / "saves/test.sav").read_text() == "saved\n"

    (install / "options.cfg").write_text("preserved\n")
    run(executable, install)
    alias = base / "launcher-link"
    alias.symlink_to(executable)
    run(alias, install)
    moved = base / "relocated folder"
    install.rename(moved)
    run(moved / "check", moved)
    assert (moved / "options.cfg").read_text() == "preserved\n" + "checked\n" * 3
    # A storage directory must not redirect writes outside the installation.
    (moved / "saves/test.sav").unlink()
    (moved / "saves").rmdir()
    (moved / "saves").symlink_to(unrelated, target_is_directory=True)
    blocked = subprocess.run([str(moved / "check")], cwd=unrelated, env=env,
                             capture_output=True)
    assert blocked.returncode == 1
    assert not list(unrelated.iterdir()), "Wrote into the caller's working directory"
print("PASS: foreign cwd, symlink launch, relocation, existing settings, local save writes,")
print("      storage environment, firmware socket path, and invalid storage destination.")
