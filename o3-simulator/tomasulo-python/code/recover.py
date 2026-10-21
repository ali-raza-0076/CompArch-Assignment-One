# function: keep every still-pending branch's RAT snapshot in sync with
# reality, so that if that snapshot is later restored, it never points to
# a tag whose producing instruction has already committed and retired
def patch_snapshots(ROB, dest_tag, value):
    for entry in ROB:
        snap = getattr(entry, 'rat_int_snapshot', None)
        if snap is not None:
            for i in range(len(snap)):
                if snap[i] == dest_tag:
                    snap[i] = value
        snap = getattr(entry, 'rat_fp_snapshot', None)
        if snap is not None:
            for i in range(len(snap)):
                if snap[i] == dest_tag:
                    snap[i] = value

# function: tags of every ROB entry issued after the given branch
def find_flushed_tags(ROB, branch_tag):
    tags = set()
    found = False
    for entry in ROB:
        if found:
            tags.add(entry.ROB_tag)
        if entry.ROB_tag == branch_tag:
            found = True
    return tags

# function: wipe every trace of the wrong-path instructions, immediately,
# so that nothing downstream in this same cycle (commit included) can act
# on them. Only when fetch is allowed to resume is deferred to satisfy the
# n+2 timing rule; cleanup itself cannot wait, since commit and memory
# writes are irrevocable.
def do_squash(flushed_tags, ROB, rat_int, rat_fp,
        rs_int_adder, rs_fp_adder, rs_fp_multi,
        ld_sd_queue, fu_int_adder, fu_fp_adder, fu_fp_multi,
        ld_sd_exe, ld_sd_mem, results_buffer, cdb,
        rat_int_snapshot, rat_fp_snapshot):
    while len(ROB) > 0 and ROB[-1].ROB_tag in flushed_tags:
        ROB.pop()

    for rs in [rs_int_adder, rs_fp_adder, rs_fp_multi]:
        for entry in rs:
            if (entry.busy == 1) and (entry.dest_tag in flushed_tags):
                entry.busy = 0

    kept = [e for e in ld_sd_queue if e.dest_tag not in flushed_tags]
    ld_sd_queue.clear()
    ld_sd_queue.extend(kept)

    for fu in [fu_int_adder, fu_fp_adder, fu_fp_multi]:
        kept_fu = [e for e in fu if e.dest_tag not in flushed_tags]
        fu.clear()
        fu.extend(kept_fu)

    if (ld_sd_exe.busy == 1) and (ld_sd_exe.dest_tag in flushed_tags):
        ld_sd_exe.busy = 0
    if (ld_sd_mem.busy == 1) and (ld_sd_mem.dest_tag in flushed_tags):
        ld_sd_mem.busy = 0

    kept_rb = [r for r in results_buffer if r.dest_tag not in flushed_tags]
    results_buffer.clear()
    results_buffer.extend(kept_rb)

    if (cdb.valid == 1) and (cdb.dest_tag in flushed_tags):
        cdb.valid = 0

    rat_int[:] = rat_int_snapshot
    rat_fp[:] = rat_fp_snapshot

# function: resume fetch once the recovery delay has elapsed
def maybe_resume(PC, cycle):
    if (PC.valid == 0) and (getattr(PC, 'resume_cycle', None) == cycle):
        PC.valid = 1
