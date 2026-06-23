# Tech stack
- C++ network models built as INET shared libraries against OMNeT++.
- Project Makefile generates `src/Makefile` via `opp_makemake`; feature configuration generates `src/inet/features.h`.
- Python tooling is exposed through `setenv` (`PYTHONPATH=python`).