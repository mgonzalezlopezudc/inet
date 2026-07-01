# Suggested commands
- Build: `make -j$(nproc)`; regenerate build files after source topology changes: `make makefiles`.
- HE/OFDMA unit tests: `export CCACHE_DISABLE=1; inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"`.