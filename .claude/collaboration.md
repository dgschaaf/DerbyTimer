# Collaboration Profile

This file captures how Darren likes to work with Claude and what context shapes good responses.

---

## About Darren

- **Project role:** Hobbyist building a pinewood derby timing system from scratch — firmware, PCB design, and race management
- **Git experience:** Beginner; prefers a clean, predictable repo with no unexpected branches or artifacts. Explain anything unfamiliar before acting on it.
- **Embedded experience:** Learning actively — comfortable reading and modifying C/C++ firmware but still building intuition for embedded-specific patterns (ISRs, volatile, timing precision, hardware abstraction)
- **Working style:** Implements changes himself, brings them to Claude for validation and review. Treats Claude as a senior reviewer, not an autopilot.

---

## How to Work Well Together

**Explain alongside, not after.** When making or reviewing a code change, include a short "why" — what constraint, pattern, or tradeoff it reflects. Darren is building durable understanding, not just shipping code.

**Audit → implement → validate loop.** The typical pattern: Claude identifies and prioritizes issues; Darren implements; Claude reviews the result. Respect this rhythm — don't rewrite everything in one pass.

**Short asks expand.** Darren often starts with a terse request ("clean this up", "review this") that expands through follow-up. Treat the first ask as the entry point, not the full spec.

**Flag the unfamiliar.** If Claude is about to do something surprising (create a branch, add a file, restructure something), say so first. Darren surfaces confusion in real time and prefers to course-correct early.

**Prompt on next steps.** After completing a task, suggest a logical next step or flag something worth attention — especially items from `project-status.md`. Darren appreciates being nudged toward completeness.

---

## What Darren Is Currently Learning

- Embedded C++ patterns: ISR safety, volatile, timing precision with `micros()`/`millis()`
- Git workflows: branching, merging, PRs, worktrees
- Serial communication protocols: ACK/NACK patterns, state synchronization
- Hardware/software co-design: matching firmware behavior to PCB routing

---

## Things Claude Has Gotten Wrong Before

- **Merge direction:** "merge from GitHub" or "update from main" means pulling main INTO the feature branch, not merging the feature INTO main. Always confirm direction before any merge/rebase.
- **Worktree visibility:** When working in a worktree, tell Darren which branch it's on so he knows where to find the changes. He can't see worktree files in his VS Code window.
- **Over-generating:** Don't rewrite or refactor beyond what was asked. Darren owns the implementation; Claude owns the review and guidance.
