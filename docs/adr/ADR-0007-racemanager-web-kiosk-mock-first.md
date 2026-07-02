# ADR-0007: Race Manager is a Python + web-kiosk app, developed mock-first

**Status:** Accepted
**Date:** 2026-07-02

## Context

The Race Manager is a full-screen kiosk application on a Raspberry Pi: event
coordinator, results display, and database manager, connected to the Finish
Controller over BLE. Two decisions had to be made before work could start on
the `feature-raceManager` branch.

**Platform.** The application needs full-screen kiosk operation, BLE central
role, bracket-tree and reveal-screen rendering, SQLite persistence, and
development largely unattended (agents working GitHub Issues with
machine-checkable verification). Candidates: Python desktop GUI (Qt/tkinter),
Node.js/Electron, or a Python backend serving a web UI to Chromium in kiosk
mode.

**Ordering.** The original plan made the Finish Controller's BLE GATT firmware
the first phase, blocking everything else. But `main` is mid-flight on the AD
architecture slices, which refactor the same finishController files -- a
long-lived branch carrying firmware edits would collect merge conflicts on
every update from main.

## Decision

**Platform: Python backend + web UI in Chromium kiosk mode.** The backend owns
BLE (`bleak`), SQLite, and all race logic; the frontend is HTML/CSS/JS served
locally. Bracket trees, roster grids, and celebration screens are much more
tractable in HTML/CSS than in desktop toolkits, and the whole app develops and
tests on a Windows PC in a normal browser -- no Pi or track hardware needed
until deployment. Qt was rejected for UI effort and learning curve; Electron
for weight on the Pi and a less-maintained BLE story.

**Ordering: mock-first, firmware last.** The GATT contract is frozen on paper
first; a Mock Finish Controller (a fake implementation of the FC link
interface, replaying scripted race events) stands in for hardware. All RPi
application milestones run against the mock. Firmware BLE work starts only
after the AD slices land in main and are pulled into the branch.

## Consequences

- Two-language stack (Python + HTML/JS) -- accepted; the UI stays thin and all
  decisions live in the pytest-covered backend.
- The FC link interface is a hard seam: nothing above it may know whether the
  mock or real BLE is connected. The real `bleak` link is one of its two
  implementations and arrives last (hardware-integration milestone).
- The mock plus scripted race-day scenarios double as the AFK verification
  harness (the Race Manager's equivalent of the firmware's L2 native tests).
- Firmware BLE edits on this branch must not begin until the AD slices are
  merged into main and pulled in -- revisit if the AD work is abandoned.
