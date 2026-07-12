# UORA plots

The UORA dashboard presents success probability, unsuccessful random-access
attempts, and per-station Jain fairness. Heavy contention should reduce success
and increase failed attempts. Adding random-access RUs should recover success,
although those RUs also consume resources that could serve scheduled users.

The unsuccessful-attempt count is `attempts - successes`; it is an outcome
measure and should not automatically be called a PHY collision.
