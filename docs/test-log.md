# Test Run Log

Hardware-free runs only (L2 native unit tests). All other layers require physical hardware.

| Date | Scope | Command | Result | Notes |
|------|-------|---------|--------|-------|
| 2026-05-26 | L2 native — 4 suites | `pio test -e native` | 30/30 PASSED | Fixed 1 bad test (`test_foul_bits_no_bleed_into_winner` → `test_foul_left_and_right_no_overlap`): foul/winner masks are separate payloads, not same byte. MinGW required; add `C:\ProgramData\mingw64\mingw64\bin` to PATH first. |
