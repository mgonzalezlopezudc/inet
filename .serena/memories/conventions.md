# Conventions
- Simulation implementation lives in `src/inet`; adding source files there requires makefile regeneration.
- Assertions/logging use OMNeT++/INET facilities; keep simulation-model changes protocol-behavior neutral unless the task explicitly asks otherwise.