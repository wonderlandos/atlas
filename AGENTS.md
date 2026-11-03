# Atlas: rules for AI agents

These rules apply to any AI agent working in this repository.

- Follow [CONTRIBUTING.md](CONTRIBUTING.md) and [atlas/README.md](atlas/README.md); they are the
  rules for people and agents alike. Conduct: [The Wonderland Doctrine](https://github.com/wonderlandos/wolos/blob/main/docs/DOCTRINE.md).
- Work only on the branch `main`, and only in `atlas/`, `tools/` and this repository's own
  documents. Never edit anything under `patches/`, never edit the branch `upstream`, never rewrite
  history someone else has pushed.
- One commit per patch, header as the commit message; check with
  `tools/apply-overlay.sh --dry-run TREE` before committing, and say so in your report. If you
  could not run it against a real tree, say that too.
- Prefer a GN argument, a policy, a launcher flag or a scripted transform over a patch; stay inside
  the 60-patch budget.
- Never push, tag or publish. The lead pushes.
- **Never contact the upstream project.** Do not open issues, pull requests, discussions or
  comments in the upstream repositories or their organisation, and do not prepare them on
  someone's behalf. Upstream does not accept contributions written by AI tools; a fix we offer
  them is written and sent by a person on our team.
- Do not name the upstream project in code, patch headers, commit messages, UI strings or
  documentation; the credits live in [README.md](README.md) and the licence files only. Inherited
  code identifiers stay as they are.
