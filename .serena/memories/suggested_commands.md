# Suggested commands
- Source OMNeT++ before INET: `source /home/user/omnetpp-6.4.0aipre2/setenv -f && source setenv -q`.
- Build: `make -j$(nproc)`; regenerate build files after source topology changes: `make makefiles`.
- HE/OFDMA unit tests: `export CCACHE_DISABLE=1; bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"`.