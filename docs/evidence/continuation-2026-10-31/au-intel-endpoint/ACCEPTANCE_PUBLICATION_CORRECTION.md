# AU acceptance publication correction

The intended concurrency and identical-state extension was not included in
commit 9dd7762c32db2831316428444ebd1acc3547c9ab: that commit carried the
older acceptance JSON. The integration CTest subsequently failed with
`missing tolerance incoherent_read_count`, which exposed the publication
mistake. This record preserves that failure and corrects the chronology.

The original scheduled, external and protected readback limits of zero, the
1e-7 normalized-curve limit and 2e-6 audio limit were published correctly in
aa3cadb4147ca9a1cd1cfe843345fcefa3d97432 before repair evaluation. Those
limits have not changed. The added concurrent-reader and identical-state
local witnesses were evaluated before their explicit extension reached the
remote repository. They are supplementary retrospective coverage, not a
prospectively registered result. The original local logs remain evidence.

This commit publishes the exact missing extension before native Intel/ARM
qualification of the repaired AU. Further checks retain every original
assertion. Repeating a local check does not change its original chronology.
