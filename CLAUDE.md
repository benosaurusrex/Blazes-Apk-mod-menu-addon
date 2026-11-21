# Claude instructions

This is https://github.com/florstedtleo-sketch/mod-sdk. Follow AGENTS.md in this repo.

`python3 tools/verify_repo.py` compares this copy with that repo (it is switched off in the file;
set `ENABLED = True` to turn it on). `python3 tools/check_mod.py` has no rules and always passes.

A mod must always be a compiled `.so` file (`out/<name>.so` from `./build.sh`). The game only loads
`.so` files, so never hand over source code, scripts or other file types as the mod.

@AGENTS.md
