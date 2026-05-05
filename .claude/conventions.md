# Conventions

---

## Code Conventions

- **New states or modes:** Add to the relevant enum in `globals.h` first, then wire into both controllers. Enums are shared; changing one side without the other will cause a mismatch.
- **Inter-controller data:** All values received over serial live in `SerialRxState rx` (extern from `serialComm.h`). Access as `rx.State`, `rx.Mode`, `rx.LeftFoul`, `rx.LeftReactionTime`, etc. Do not create parallel globals.
- **Planned features:** Mark with `// future:` inline comments. This makes them easy to grep and signals intent without blocking compilation.
- **State machine entry/exit:** Both controllers use `stm.entry` / `stm.exit` flags for one-shot setup and teardown logic per state. Always clear `stm.entry = false` at the top of the entry block.
- **TX transactions:** After any `txStatus` result of `TX_ACKED`, `TX_TIMEOUT`, or `TX_FAILED`, call `resetTxState(msgID)` to clean up the TX state machine.

---

## File Encoding

- Use **plain ASCII** in all source and documentation files.
- Avoid: non-breaking hyphens (`\xE2\x80\x91`), smart quotes (`""`), em/en dashes, or any Unicode lookalikes.
- These cause silent encoding mismatches when files are opened across tools (Arduino IDE, VS Code, git).

---

## Git Workflow

- **Default branch:** Work in `main` or the current branch unless a worktree is specifically needed for isolation.
- **Worktrees:** When Claude operates in a worktree, always state which branch the edits are on so Darren knows where to find them (worktree files are not visible in the main VS Code window).
- **Merge direction:** "update from main" or "merge from GitHub" means pulling main's changes INTO the feature branch (`git merge main` or `git rebase main`). It does NOT mean merging the feature into main. Always confirm direction before executing.
- **PR creation:** `gh` CLI is installed. Use `gh pr create` for pull requests. Authenticate with `gh auth login` if needed.
- **Commit messages:** Describe the *why*, not just the what. One sentence is usually enough.

---

## Documentation Conventions

- Human-facing design docs live in `docs/` (startController.md, finishController.md).
- Claude-specific references live in `.claude/` (this folder).
- Keep `project-status.md` current — update it as part of any session that advances or closes an item.
- Hardware test protocols live in `hardware/hwTest/*.md`.
