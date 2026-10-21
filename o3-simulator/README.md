# Tomasulo Algorithm Implementation — Multi-Issue + Branch Prediction

In-order multi-issue (1-4 instructions/cycle) added to an existing
single-issue Tomasulo simulator (RAT, ROB, per-unit reservation stations,
load/store queue with forwarding and disambiguation, single CDB), plus
the branch unit the assignment specifies: a one-bit predictor per branch,
an 8-entry BTB indexed by the branch's low 3 PC bits, prediction at
issue (no stall), resolution at end of EX, and full misprediction
recovery (RAT restore, ROB/RS/load-store-queue cleanup, refetch at
cycle n+2). Full design, the two correctness bugs found and fixed while
building recovery, and results in the report:
[`../report/report.pdf`](../report/report.pdf) (Part 2).

## Run

```
cd tomasulo-python/code
python main.py [instruction-file] [issue-width]
```

`instruction-file` defaults to `code.in`, `issue-width` defaults to 1 (max 4).

```
python main.py                         # code.in, issue width 1
python main.py code.in 4               # code.in, issue width 4 (2 real mispredictions)
python main.py test_rs_full.in 4       # integer-adder RS saturation
python main.py test_parallel_fu.in 4   # 4 FUs issuing in parallel
python main.py test_mixed_ops.in 4     # Sub / Sub.d opcodes
python main.py test_ldsd_full.in 4     # load/store queue saturation
python main.py test_branch_squash.in 4 # wrong-path store must never reach memory
```

Output: per-instruction ISSUE/EX/MEM/WB/COMMIT cycle table, followed by the
final integer/FP register file and non-zero memory contents.

## Results

Verified by diffing final register/memory state across issue widths 1-4,
for six programs chosen to stress different parts of the implementation
independently. All six produce identical final state at every issue
width, with or without branches involved — the property that actually
matters, since issue width and speculation should only ever change
timing, never outcome.

| Test | W=1 | W=2 | W=3 | W=4 | Mispredictions |
|---|---|---|---|---|---|
| `code.in` (given) | 39 | 38 | 38 | 39 | 2 |
| `test_rs_full` | 9 | 11 | 11 | 11 | 0 |
| `test_parallel_fu` | 21 | 20 | 19 | 19 | 0 |
| `test_mixed_ops` | 26 | 24 | 24 | 23 | 0 |
| `test_ldsd_full` | 39 | 39 | 39 | 39 | 0 |
| `test_branch_squash` | 9 | 9 | 9 | 9 | 1 |

`test_rs_full` is the one where wider issue is *slower* (9 vs 11 cycles):
it only exercises a single integer-adder functional unit, so bursting
instructions in faster than that one unit can drain them doesn't help.
`test_branch_squash` specifically races a wrong-path store against
misprediction detection; its data never reaches memory at any issue
width, which is the test that caught (and now guards against) one of the
two recovery bugs described in the report.

See the report for the full per-test cycle tables, the instruction status
table format, and the two bugs (a stale RAT recovery snapshot, and a
squash/commit timing race) found and fixed during development.
