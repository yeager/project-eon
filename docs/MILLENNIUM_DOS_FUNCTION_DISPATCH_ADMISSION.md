# Millennium DOS function-dispatch admission

The common scaled-dispatch gate is deliberately independent of active runtime
reachability. It receives three already separate facts:

- the exact English `2200AD.EXE` bytes, which are reparsed and authenticate the
  ten-entry function table;
- a post-overlay loop stopped at its typed terminal dispatch boundary; and
- an explicit observation naming call site, dispatcher, index, and handler.

Admission succeeds only when the loop reports `$d40a → $76f1`, its recovered
index is within the exact ten-entry table, and all four observed fields match
both that boundary and the handler address parsed from original media. No
field is defaulted, no table index is selected, and no call return or handler
semantics are inferred.

The result is a copy containing only acceptance, index, handler address, and a
bounded diagnostic. It owns no original bytes and grants no input, rendering,
audio, or gameplay capability. In particular, a positive unit admission with
an independently constructed exact-media loop is not evidence that today's
launcher can reach the loop. Active runtime integration must retain its own
predecessor-state and lifecycle checks.

The native genuine-media harness exercises an observed index-6 boundary and
rejects mismatched handler, index, and call-site observations. F6, F7, F8 and
F10 coordinator gates call the same validator, then retain their narrower
handler-specific expected index/address checks and error labels before a
session is constructed. The tests use the authenticated executable already
held in memory and never create synthetic commercial data.
