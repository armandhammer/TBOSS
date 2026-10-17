## What this changes

<!-- One or two sentences; link the issue if there is one. -->

## How it was tested

- [ ] Tests pass (`run_tb_tests.py` with and without `--sanitize`, `check_notes.py`,
      the Python and web tests; the *Tests and build* check runs them all)
- [ ] On a real board (which? what did you try?), or: not tested on hardware
- [ ] The manual describes any user-visible change

The build's safety check must pass: no debug pins, option bytes, low-power modes, flash
writes outside the settings sector, or writes to the external memory chip.
