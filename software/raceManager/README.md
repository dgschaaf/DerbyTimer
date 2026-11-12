# Race Manager

Source of truth for: setting up and testing the Race Manager Python package.
The design (features, race modes, milestones) lives in
[docs/raceManager.md](../../docs/raceManager.md), and the platform decision in
[ADR-0007](../../docs/adr/ADR-0007-racemanager-web-kiosk-mock-first.md).

The Race Manager is the event-level race day coordinator. It runs as a
full-screen kiosk app on a Raspberry Pi and handles the roster, race modes,
heat scheduling, standings and results storage. It talks to the Finish
Controller over BLE. Development is mock-first: everything runs and is tested
on a normal PC, with no Pi or track hardware.

## Layout

```
software/raceManager/
  pyproject.toml        package metadata, dependencies, pytest config
  src/race_manager/     the package (src layout)
  tests/                pytest suite
```

## Setup

You need Python 3.11 or newer. Run these from `software/raceManager/`.

Windows (PowerShell):

```
py -m venv .venv
.venv\Scripts\Activate.ps1
python -m pip install -e ".[dev]"
```

Linux / macOS / Raspberry Pi OS:

```
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -e ".[dev]"
```

The editable install (`-e`) pulls in pytest and links the package into the
environment, so edits under `src/` take effect without a reinstall.

## Running the tests

With the environment active, from `software/raceManager/`:

```
pytest
```

pytest takes its configuration from `pyproject.toml` (`testpaths = tests`,
`pythonpath = src`), so it also runs straight from a checkout without the
editable install, as long as pytest itself is installed. From the repo root,
`pytest software/raceManager` works too.
