- [ ] One commit per change; a patch commit adds or changes exactly one patch file and its series line.
- [ ] Nothing under `patches/` is edited; the branch `upstream` is untouched.
- [ ] `tools/apply-overlay.sh --dry-run TREE` prints `ok` for every patch (say which tree), or I
      say why I could not run it.
- [ ] The patch header says why a GN argument, a policy, a flag or a transform was not enough.

---

[what changes for the person using Atlas, and how it was tested]
