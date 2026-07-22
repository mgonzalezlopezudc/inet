---
name: inet-architectural-requirements
description: Apply INET architectural requirements, naming conventions, exception ledgers, enforcement checks, and source-file sealing policy. Use when Codex designs, implements, refactors, audits, or reviews C++, NED, MSG, configuration, build, or package changes under src/inet; evaluates INET dependency direction, contracts, composition, protocol interaction, packet representation, observability, extensibility, determinism, testing, or naming; or checks, proposes, grants, or removes a seal.
---

# INET architectural requirements

## Enforce the sealing guard first

Before adding, editing, moving, renaming, or deleting anything under `src/inet/`:

1. Read [sealing.md](references/sealing.md) and [sealing-status.md](references/sealing-status.md).
2. Resolve the target path against both exact-file entries and recursive ancestor-directory entries. Paths in the status file are relative to `src/inet/`.
3. Treat a new file under a sealed directory as sealed. Treat generated `_m.h` and `_m.cc` siblings as covered by the source `.msg` seal.
4. If any target is sealed, stop before modifying it, name the specific file, and ask the user for explicit permission for that file in the current conversation. A broad implementation or refactoring request is not sufficient permission.
5. After permission, keep the file sealed and re-audit the resulting change. Never remove or add a sealing entry unless the user explicitly requests that sealing action.

Files are unsealed by default, but establish that fact from the current status file rather than memory.

## Load only the references needed

- Read [requirements.md](references/requirements.md) when assessing user-facing modeling scope, composition, execution, results, visualization, emulation, documentation, or compatibility.
- Read [architectural-requirements.md](references/architectural-requirements.md) before designing or reviewing production changes. Search by stable identifiers such as `AR-ORG`, `AR-MOD`, `AR-PKT`, `AR-COM`, `AR-LIFE`, `AR-QUEUE`, `AR-OBS`, `AR-CFG`, `AR-EXT`, `AR-BUILD`, and `AR-QUAL` to focus the review.
- Read [ieee80211-architectural-requirements.md](references/ieee80211-architectural-requirements.md) when reviewing production changes to 802.11 code.
- Read [naming-conventions.md](references/naming-conventions.md) for every new or renamed package, file, NED type, gate, parameter, signal, statistic, MSG type or field, C++ identifier, configuration, feature, directory, test, icon, or registered name.
- Read [architecture-exceptions.md](references/architecture-exceptions.md) before reporting dependency violations, and [naming-exceptions.md](references/naming-exceptions.md) before reporting naming violations. Treat these as ledgers of existing reality, not as rules to copy into new code.
- Read the [agent-review checklist](references/enforcement/agent-review-checklist.md) for semantic review of a diff. Consult the staged [clang-tidy configuration](references/enforcement/.clang-tidy) for mechanically checkable C++ naming rules.
- Read the [agent-review checklist for IEEE 802.11](references/enforcement/ieee80211-agent-review-checklist.md) for semantic review of 802.11-specific changes.
- Use reports under `references/reports/` only as prior audit evidence for the exact scope they cover. Revalidate any claim that may have changed.

## Apply the requirements

1. Establish the task scope and affected paths. Distinguish design, implementation, focused review, naming audit, architecture audit, and sealing work.
2. Pass the sealing guard before making any `src/inet/` change.
3. Map the proposed or actual change to the applicable `R-*` and `AR-*` identifiers. Cite identifiers rather than paraphrasing an unnamed preference.
4. Inspect the checked-out C++, NED, MSG, configuration, registration, build, and test artifacts that establish actual behavior. Do not infer compliance from filenames alone.
5. Check both exception ledgers. Do not re-flag a sanctioned exception or an already-recorded open violation as a new finding.
6. Prefer the smallest change that satisfies the applicable contracts, dependency direction, composition, configuration, observability, extensibility, determinism, naming, and testing rules.
7. Keep unrelated existing violations out of the patch. Report them separately when relevant.
8. Validate in proportion to the change, preserving exact commands, working directory, exit status, and artifact paths.

## Audit and validation

Run the include-graph fitness function from the repository root for architecture-sensitive changes:

```sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh src/inet/<focused-subtree>
```

Use the focused form when reviewing a bounded subtree. The script intentionally reports known, non-allowlisted violations; reconcile its output with `architecture-exceptions.md` instead of treating every nonzero exit as newly introduced.

For a semantic diff review, apply every item in the agent-review checklist and emit exactly one of `PASS`, `FLAG`, or `QUESTION` per item. Scope findings to the diff, cite `file:line`, and end with the prescribed `REVIEW: n PASS, n FLAG, n QUESTION` footer.

Use the staged `.clang-tidy` file as a reference or explicit config when appropriate; do not copy it to the repository root or weaken it merely to match existing violations. Run the relevant build and test categories required by `AR-QUAL-TESTS`. Follow the repository's dedicated build, unit-test, simulation, and fingerprint skills for their execution details.

## Maintain exception ledgers deliberately

- Classify deliberate architecture exceptions as `AS-*` and genuine architecture violations as `AV-*`.
- Classify permanent naming exceptions as `NS-*` and rename candidates as `NV-*`.
- Preserve stable identifiers and statuses; never reuse or silently delete an identifier.
- Do not edit either ledger merely because an audit found something. Report the proposed row and ask for explicit user approval before changing these skill reference files.
- Do not weaken a target requirement or naming rule to conform to legacy code.

## Seal only after a complete audit

When the user asks to seal a file or directory:

1. Audit the entire requested scope against the architectural requirements, naming conventions, applicable deterministic checks, and semantic review checklist.
2. Present the audit before proposing or recording the seal.
3. Require every finding to be fixed or explicitly accepted and recorded as an `AS-*` or `NS-*` exception. Never seal over an open `AV-*` or `NV-*` violation.
4. Add the exact file path or trailing-slash recursive directory path to `sealing-status.md` only after explicit user approval, in the same commit as the compliant state.

If a violation is discovered later in a sealed file, report it and request permission before fixing it; the seal remains in force.

## Report the outcome

Include:

- scope and seal status;
- applicable `R-*` and `AR-*` identifiers;
- findings with repository-relative `file:line` evidence and ledger disposition;
- exact validation commands and exit statuses;
- unresolved questions or required approvals;
- a concise compliance verdict.

Separate verified facts from recommendations. Do not claim that a dependency, behavior, naming issue, or seal is compliant without source or validation evidence.
