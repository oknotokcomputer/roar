# Perfetto - System profiling, app tracing and trace analysis

Perfetto is a production-grade open-source stack for performance
instrumentation and trace analysis. It offers services and libraries and for
recording system-level and app-level traces, native + java heap profiling, a
library for analyzing traces using SQL and a web-based UI to visualize and
explore multi-GB traces.

See https://perfetto.dev/docs or the /docs/ directory for documentation.

This repository is a crosvm-specific copy of a google-internal perfetto
repository modified for use on Windows with Rust. It is not a full fork of the
external repository as it does not preserve the repo commit history. It is
instead checkpointed from a specific (internal) git commit hash.
