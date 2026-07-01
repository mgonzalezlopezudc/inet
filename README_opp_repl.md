# Preferred Persistent MCP Setup

For long debugging sessions, prefer a persistent opp_repl process controlled
through its MCP socket. Start it before launching the AI coding agent
or before expecting it to use the bridge:

```bash
cd /home/user/opp_repl
. setenv
export OMNETPP_ROOT=$HOME/omnetpp-6.4.0aipre2
export __omnetpp_root_dir=$HOME/omnetpp-6.4.0aipre2
export INET_ROOT=$HOME/omnetpp_ws/inet
opp_repl --load @opp -p inet --mcp-socket
```

Configure the MCP-capable coding agent with the stdio bridge:

```bash
opp_repl_mcp_bridge
```

The bridge connects stdio MCP clients to the persistent opp_repl Unix-domain
socket. This lets the agent execute Python inside the already-running REPL and
reuse loaded projects, helper variables, result objects, cached discovery, and
debugging state across calls. The default socket path is chosen automatically
when `opp_repl --mcp-socket` and `opp_repl_mcp_bridge` are both used without an
explicit path; run `opp_repl_mcp_bridge --help` to see the resolved path and
client configuration examples.

Keep the two MCP layers distinct:

- `opp_repl_mcp_bridge` talks to the persistent Python opp_repl session.
- `QtenvMCPClient` talks to a live Qtenv simulation at
  `http://127.0.0.1:<port>/mcp`.

Use the bridge as the control plane and Qtenv's MCP endpoint for live
simulation inspection.
