# Branch Predictor Implementation

Pentium M-style hybrid branch *direction* predictor (bimodal + tagged
4-way global predictor), implemented in
[`cbp2-infrastructure-v2/src/my_predictor.h`](cbp2-infrastructure-v2/src/my_predictor.h)
(`pm_predictor`), plus a complete predictor with branch target prediction
for extra credit (`cpm_predictor`). Full design, testing, and results in
the report: [`../report/report.pdf`](../report/report.pdf) (Part 1).

## Build

```
cd cbp2-infrastructure-v2/src
make
```

Produces `predict` (required predictor) and `predict_extra_credit`
(extra-credit predictor with target prediction).

## Run

Single trace:
```
./src/predict traces/164.gzip/gzip.trace.bz2
./src/predict_extra_credit traces/164.gzip/gzip.trace.bz2
```

All 19 benchmark traces + average MPKI (the original `run` script is
`csh`, unavailable on Windows, so a `bash` equivalent is included):
```
bash run.sh traces
```

## Results

Average over all 19 provided SPEC traces:

| Predictor | Direction MPKI | Target MPKI |
|---|---|---|
| `pm_predictor` (required: bimodal + 4-way global) | 6.604 | — |
| `cpm_predictor` (extra credit: + BTB + indirect target cache) | 6.604 | 7.393 |

Compared trace-by-trace against the reference `gshare` predictor (32K
entries, no tagging) across all 19 traces (average 6.306) — close in both
directions, with the hybrid predictor using well under a third of the
counter storage. Full per-trace numbers, the hand-verified predictor
trace, the 2-way→4-way tuning experiment, and the design/debugging
narrative (including a type bug found and fixed in the given
`predictor.h`) are in the report.
