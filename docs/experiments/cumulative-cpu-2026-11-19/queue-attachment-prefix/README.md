# Exact parameter attachment hint for host automation

A queue may arrive in any order. First preserve the existing first-lane shortcut,
then try the current attached-lane count as a prefix hint and confirm exact
parameter-object identity before using it. Fall back to the full roster search for
out-of-order, duplicate, sparse or foreign parameter objects. No callback state,
parameter metadata, float operation or queue lifetime changes.

Against the preceding exact-FIR product, 120 real VST3 native/translated
observations compared 12 cases with repeated/count-additivity checks and bytewise
public output/state in each runtime. The [four non-automation cases](core.json)
are unchanged in instruction, read and write counts. With 120 ordered queues,
the [processor callback](queues.json) saved 140,432 guest instructions
(0.247%) and 27,608 reads over four callbacks. The
[adversarial order cases](order.json) retained all observations: instruction
counts fell and writes stayed flat, with 8 to 952 extra reads.

Unique valid slots are compile-time checked. A real two-instance test checked
every host parameter in both ownership roles, same-index foreign parameters,
duplicate attachment, endpoints and reset/reprepare: 5,944 checks per arm passed.
No native CPU time or target verification is implied.
