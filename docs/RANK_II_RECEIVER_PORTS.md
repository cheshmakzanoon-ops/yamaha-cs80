# Rank-II preset receiver ports and eight-voice harness

2026-10-29. **Native-source transcription independently reviewed.** This extends the [rank-I receiver ledger](PRESET_RECEIVER_PORTS.md) with actual T53/T54 and M21–M28 wiring. It is topology evidence, not calibrated control laws, a loaded preset solution or permission to replace factory voicings. No production code or preset values changed.

## Findings and boundary

All 26 rank-II first-hop destinations are identified from T53/T54, with every numbered unprimed bus linked reciprocally between those two boards. The M21–M28 sheets establish the individual harness chain for PW, G1/G2, IL/AL, AF/DF, LP1/LP2, AA/DA/SL and the separate processed RF/RA release outputs. This is stronger than assuming eight loads from rank-I symmetry. It does not supply the electrical current drawn by any receiver or prove identical voice states.

Touch routing is **not** a simple board-number substitution: rank-II brilliance uses TRG1/2's second channels, while its level controls go to TRG4/5. Several actual source annotation conflicts are preserved below. The existing 547 classified / 25 excluded matrix-cell inventory is unchanged.

Symbols in parentheses are physical terminal numbers; primes matter. `W36`, `W38`, `M48`–`M51`, `R40`, `BA27/28` and `MBK63` refer to the native evidence table below. No PANEL/memory endpoint is assumed to be an always-connected grounded resistance.

## The 26 outputs

W36 and W38 explicitly connect corresponding unprimed terminals to each other for every row. Listed memory endpoints are printed harness names, not an electrical load model.

| Out / control | First hop and additional bus branch | Demonstrated downstream boundary / remaining work |
|---|---|---|
| 1 Speed, 1(33) | SUB-VC2(18); MBK-M4-VR1′ | SUB speed converter law/loading and memory isolation unresolved |
| 2 PWM, 2(32) | SUB-D2(49); MBK-M4-VR2′ | M48 separately identifies SUB-P2(50) → M21 PWM41, chained to M28; D2→P2 transfer still to be solved |
| 3 PW, 3(31) | M21-PW40 | Eight-voice PW chain to M28, then MBK-M4-VR3; internal PW current law unresolved |
| 4 Square, 4(30) | M21-G1(45) | Eight-voice G1 chain to M28, then MBK-M4-SW1; no grounded matrix-shunt assumption |
| 5 Saw, 5(29) | M21-G2(44) | Eight-voice G2 chain to M28, then MBK-M4-SW2; switching threshold/current unresolved |
| 6 Noise, 6(28) | SUB-C2(41); MBK-M4-VR4′ | M48 separately shows SUB-NO2(39) → M21 NI43, chained to M28; C2→NO2 gain/current law unresolved |
| 7 HPF, 7(27) | R52-FH55 and BA-fo7(21′) | Eight R52 FH branches to M21–M28 F1(51); additional BA-fo11 joins fo7; loading laws unresolved |
| 8 RESH, 8(25) | R52-QH57 | Eight R52 QH branches to M21–M28 Q1(52); passive/current laws need dedicated R39/M41 solve |
| 9 LPF, 9(24) | R52-FL55′ and BA-fo8(23′) | Eight R52 FL branches to M21–M28 F2(55); additional BA-fo12 joins fo8 |
| 10 RESL, 10(23) | R52-QL15′ | Eight R52 QL branches to M21–M28 Q2(57); preserve printed 15′, not 55′ |
| 11 IL, 11(22) | M21-IL58 | IL chain to M28; endpoint lettering MBK-M2-VR9′ on M51 needs reciprocal memory reconciliation |
| 12 AL, 12(21) | M21-AL59 | AL chain to M28; M51 prints MBK-M2-VR10′; do not silently replace by M4 |
| 13 VCF attack, 13(19) | M21-AF62 | AF chain to M28; endpoint MBK-M4-VR11 |
| 14 VCF decay, 14(18) | M21-DF61 | Terminal61 chain to M28; M51's preceding-end label says RF61, conflicting with M27 DF61 |
| 15 VCF release, 15(17) | SUB-RF′57; MBK-M4-VR13′(4) | Separate SUB-RFO′62 → M21 RF60 → eight-voice RF chain; not raw preset bus wired directly to RF60 |
| 16 VCF level, 16(16) | M21-LP1(27) | LP1 chain to M28 → MBK-M4-VR14; W38 says T54 origin, M48 prints T53 origin, electrically joined by W36/W38 bus |
| 17 Sine, 17(15) | M21-LP2(24) | LP2 chain to M28 → MBK-M4-VR15 |
| 18 VCA attack, 18(13) | M21-AA67 | Terminal67 chain to M28 → MBK-M4-VR16; M51 predecessor label looks `1A`, while M27 labels AA |
| 19 VCA decay, 19(12) | W38 labels M21 `1A(66)` | M48 explicitly names DA66 and connects T54 output19; DA chain to M28, where MBK-M2-VR17′ is printed |
| 20 VCA sustain, 20(11) | M21-SL64 | SL chain to M28 → MBK-M4-VR18 |
| 21 VCA release, 21(10) | SUB-RA′58; MBK-M4-VR19′-4 | Separate SUB-RAO′63 → M21 RA65 → eight-voice RA chain; SUB nonlinear/switching transfer remains required |
| 22 VCA level, 22(9) | R52-LV93′ | Eight R52 LC branches to M21–M28 LC31; LV distribution includes other controls, not a direct LI voltage |
| 23 Initial brilliance, 23(7) | TRG1-IBII66; MBK-M4-VR21′ | Distinct second brilliance channel on TRG1; per-voice current/transfer and full TRG fanout unresolved |
| 24 Initial level, 24(6) | TRG4-ILII4; MBK-M4-VR22′ | TRG4, **not** a presumed second ILI input on TRG1 |
| 25 After brilliance, 25(5) | TRG2-ABII66; MBK-M4-VR23′ | Distinct second brilliance channel on TRG2; full current law unresolved |
| 26 After level, 26(4) | TRG5-ALII42; W38 memory label looks `MBK-M44-VR24′` | TRG5, **not** TRG2; preserve anomalous memory lettering pending reciprocal check |

The common M circuit's internal pins/passive values are already recorded in the rank-I ledger. These wiring pages establish which rank-II terminals reach that shared M circuit; this pass does not manufacture an IG00152/IG00159/IG00151/IG00156 current law or assert that component grades match across voices.

## Individual harness closure

The following chain is visible at **each individual board**, not inferred from matching endpoints:

`M21 ↔ M22 ↔ M23 ↔ M24 ↔ M25 ↔ M26 ↔ M27 ↔ M28`.

M48 contains M21/M22; M49 M23/M24; M50 M25/M26; M51 M27/M28. For each interior board, both adjacent names are printed for PW40, PWM41, NI43, G2(44), G1(45), IL58, AL59, RF60, DF61, AF62, SL64, RA65, DA66, AA67, LP2(24) and LP1(27). End boards show the originating source and the terminating memory links where present. RF60 and RA65 stop at M28 without a memory continuation on that terminal; their memory controls join upstream on SUB. PWM and noise similarly originate from processed SUB outputs, not the raw depth/noise preset nodes.

Named branch outputs from R52 are separate per-voice connections, not the above daisy chains:

| Voice | FH→F1(51) | FL→F2(55) | QH→Q1(52) | QL→Q2(57) | LC→LC31 |
|---|---|---|---|---|---|
| M21 | FH1(74) | FL1(75) | QH1(6) | QL1(5′) | LC1(99) |
| M22 | FH2(76) | FL2(77) | QH2(11) | QL2(6′) | LC2(98) |
| M23 | FH3(78) | FL3(79), sheet says T52 | QH3(12) | QL3(11′) | LC3(97) |
| M24 | FH4(80′) | FL4(80) | QH4(17) | QL4(16) | LC4(96) |
| M25 | FH5(81′) | FL5(81) | QH5(89) | QL5(18) | LC5(95) |
| M26 | FH6(82′) | FL6(82) | QH6(90) | QL6(90′), sheet says R51 | LC6(95′) |
| M27 | FH7(83) | FL7(84) | QH7(91) | QL7(91′) | LC7(94) |
| M28 | FH8(36) | FL8(37) | QH8(92) | QL8(92′) | LC8(94′) |

R40's rank-II wiring column reciprocally identifies the M21–M28 F1/F2, Q1/Q2 and LC destinations. Its body/terminal labels must be followed together with the marked R52 entries; they cannot be read as one unqualified rank-I board. R40 supports interpreting the M23 `T52-FL3` and M26 `R51-QL6` labels as conflicting annotations, not evidence for cross-rank filter control. Preserve both literal labels until a revision/continuity check closes them.

This establishes the eight destinations and harness endpoints. It does **not** rederive the R39 resistor graph, the total source-bus admittance or an eight-times-identical receiver approximation. A numerical model must retain each actual port and all other R52/TRG/shared-source branches.

## Rank-II BA cutoff paths

Native BA27 shows IC8 halves following fI7(22)/fI8(24), with output1/7 fed back to their negative inputs **before** the output diodes. Diode cathodes terminate at fo7(21′)/fo8(23′). BA28 connects these inputs to M2 VR5-2/VR7-2 and these outputs to T53 outputs7/9. This is the same explicitly traced type of diode-isolated memory follower as the rank-I branches, not a passive grounded shunt.

A second pair uses IC10: fI11(55) / fI12(59), followed by diodes to fo11(54) / fo12(57′). BA28 joins fo11 to fo7 and fo12 to fo8. BA28 prints **M3-VR5-2** at fI11, while MBK63's top bank labelled M4 sends VR5 to **BA-fI11(55)**. This contradiction is retained; the reciprocal M4 drawing supports the M4 interpretation but does not justify erasing BA28's M3 annotation. fI12 is explicitly M4-VR7-2 in BA28 and MBK63.

PANEL rank II uses BA IC6 outputs fo3(60′) / fo4(62′) into T53 **primed**7′(26′) /9′(23′), separate from the unprimed factory bus. These output paths have no BA output diode in the drawn IC6 follower bank; the following matrix isolation is a different element. The native `ba27-follower-supplies.png` crop recorded in the existing BA ledger was also inspected for these IC6 connections; this pass does not rederive selection-state laws.

Memory and PANEL reference rails, source-driver current laws, diode I–V and inactive-state behavior remain unresolved. No constant diode drop or ideal op-amp output is asserted.

## Annotation conflicts requiring preservation

- W38 `1A(66)` versus M48's DA66; the physical terminal and reciprocal output19 link agree.
- M48 LP1 says T53-16 whereas W38 says T54-16. W36/W38 join these same-number buses, but physical harness-origin lettering is not identical.
- M49 M23-F2 names T52-FL3(79), whereas R40 routes R52's corresponding branch to M23-F2.
- M50 M26-Q2 names R51-QL6(90′), whereas R40's R52 wiring column routes its QL6 to M26-Q2.
- M51 M28-DF61 names predecessor M27-RF61 (M27 has RF60 and DF61), and M28-AA67 names predecessor `1A67` rather than AA67. Adjacent pin-number continuity is clear; label conflicts remain.
- M51 uses MBK-M2 names for M28 IL/AL/DA endpoints amidst M4 endpoints. Do not assume or silently repair which memory path/revision those labels describe; the full MBK reciprocal endpoint audit remains open.
- BA28 fI11 calls out M3-VR5-2 while MBK63 M4-VR5 calls out fI11(55).
- W38 row26's apparent M44 lettering is not promoted to a fifth memory or silently rewritten.

## Remaining bounded work

1. Independent review completed for all 26 first-hop rows, eight board chains, R52 branch table and BA paths. Ten originals and all sixteen stored/regenerated crop hashes matched. Numerical interpretation remains outside that review.
2. Resolve the explicit MBK endpoint annotations against the complete M2/M4 schematic/wiring and revisions. Verify the reference/selected rails before computing any source impedance.
3. Trace SUB VC2/D2/C2 input-current and output transfers, TRG rank-II brilliance/level routes, and the complete R52 Q/LV passive networks. Existing release topology evidence does not supply quantitative SUB diode/transistor laws.
4. Use the shared M circuit only with explicit individual chip/grade/trim/current boundaries. No presumed symmetric loaded solve and no numeric control-to-sound mapping are authorized by this graph alone.

## Primary provenance

Original filenames for new pages38/40/48–51 were confirmed from the [Yamaha service-scan index](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/index.html). Previously acquired T53, BA and MBK originals were reused. All originals are 5100×3300. Native crops were generated with Pillow `Image.open(...).crop(box).save(...)`, no resizing/conversion. Research artifacts are local inputs, not proposed repository payloads; the recorded hashes/rectangles allow reproduction.

| Original | URL | SHA-256 |
|---|---|---|
| W36 | [source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/36%20-%20T53%20Circuit%20Board%20%26%20Wiring.jpg) | `9a906d74022b923842f11462af1931740c847f2f21e0bb53a95a3158411ce4f4` |
| W38 | [source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/38%20-%20T54%20Circuit%20Board%20%26%20Wiring.jpg) | `d1f3749203eb9cec8dc177a09321bd3116bfc34faa1454f18bd5959f3ae19423` |
| R40 | [source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/40%20-%20R51-52%20Circuit%20Board%20%26%20Wiring.jpg) | `b76ad67725a661ab416121210f771e6f3b8c6ce57d2221d9dec81a3e79156db7` |
| M48 | [source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/48%20-%20M21-22%20Circuit%20Board%20%26%20Wiring.jpg) | `72e1950ca11c72c9ababf736f8d2dc0fd8b7d4578850265414f1da29572ea464` |
| M49 | [source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/49%20-%20M23-24%20Circuit%20Board%20%26%20Wiring.jpg) | `a97f049b3d1f8cb58d389b543b2c82998364a28060c62473017b50c7fee79783` |
| M50 | [source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/50%20-%20M25-26%20Circuit%20Board%20%26%20Wiring.jpg) | `ff57eeee153938ee8c3e4723c7674e567f9e9761a40523d5062567303c973413` |
| M51 | [source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/51%20-%20M27-28%20Circuit%20Board%20%26%20Wiring.jpg) | `829a06141de2e808e99fa7448b0ad92b263dc023d5f3f6c4959bfd58d4bcf241` |
| BA27 | [source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/27%20-%20BA%20Circuit%20Diagram.jpg) | `3a6cc81d8ac8895f97e85c3e5ccb0a2b978879f53a5804753d5993d77684204e` |
| BA28 | [source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/28%20-%20BA%20Circuit%20Board%20%26%20Wiring.jpg) | `88614905fecf5b41a290a108586ee2519ea08990d125169bc51fbc3be61c1884` |
| MBK63 | [source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/63%20-%20Panel%201%20%28MKB%29%20Circuit%20Diagram.jpg) | `6050a23f85823be719aab033fd90ed3d4f1038409e38c089a64c96b658a08fcb` |

| Native crop | Source | Box | SHA-256 |
|---|---|---|---|
| `t53w-terminals.png` | W36 | `[1690, 560, 2630, 2500]` | `3fef7859a1870ec74f260098898c439c17059f37421e2cd6346db5e9f598c0f1` |
| `t54w-terminals.png` | W38 | `[1750, 650, 2650, 2550]` | `c423213dbbc49d9cf7eb7c856a739150cad376d53aa5f865aacf2e4e03c1fa6a` |
| `r40-rank2-left.png` | R40 | `[600, 950, 1500, 2470]` | `a798ed5d38b4c58ebb120a4bf37f4ebe0bc4a039ce7954e6066a935eb3f1187d` |
| `r40-rank2-right.png` | R40 | `[3550, 500, 4900, 2900]` | `c4c56b2107efbd0f4c40efc4b34d481625d29295794bd9ea81b02300526310d2` |
| `ba27-rank2-cutoff.png` | BA27 | `[1820, 2300, 3540, 2800]` | `ca732cbbff8904351af824108cb74e768a7b66f6f1211be25082179c6979159c` |
| `ba28-rank2-cutoff-right.png` | BA28 | `[3430, 1720, 5050, 2200]` | `95ce3df6b2960f35526b0b25e09f66b2cedf756f1ecc260565a767814ea5d24b` |
| `ba28-rank2-cutoff-left.png` | BA28 | `[450, 1250, 2200, 2250]` | `87ab14fa3ce00deda92a17a8dc970b86ef9ddddd0ecc28a83eb3796b9570bc1a` |
| `mbk63-m4-cutoff.png` | MBK63 | `[1700, 650, 2360, 1190]` | `2e3922a6252a8b29e1bc9bafb3f3234e5162c77b605957415a461fd40a27d9be` |
| `m48-left.png` | M48 | `[610, 690, 2450, 2740]` | `fb0d3046e66919b8366f0f837d65c7972dbb87d912fc1dd880de83c5050ab749` |
| `m48-right.png` | M48 | `[2680, 690, 4390, 2740]` | `85132ea16cb03f65c1338ba774509d66dd69b1f6ecf7d20ad9372808e6c3b94d` |
| `m49-left.png` | M49 | `[610, 690, 2700, 2740]` | `6fcb4fc6f1a5a6e1aa9ed60db6b0879f18b1fcda22dcf5c8c36e8085cd8eadaf` |
| `m49-right.png` | M49 | `[2680, 690, 5000, 2740]` | `9e9d66a16d6961c59c06cb77435f9516b48cf54bb0d92bf36e468ef2ddc6c1e3` |
| `m50-left.png` | M50 | `[610, 690, 2450, 2740]` | `296219009f643af649d0c6e0f83f7efd9bee768193c1fc7fe908a51b04fe5182` |
| `m50-right.png` | M50 | `[2680, 690, 4390, 2740]` | `dcd4c497e9d7b79cfdb68023c607ab284f5403c3c60396278de9709c843f8694` |
| `m51-left.png` | M51 | `[610, 690, 2700, 2740]` | `a0f0a9b7d1b52f24b117a4d86dd36fc121ce51036e033f66af1713d878ef57e3` |
| `m51-right.png` | M51 | `[2680, 690, 5000, 2740]` | `bafb8b73ef36f35e68eca7bad816864a66bbea69f2f49a9ef641709c854d4208` |

Local manifest: `artifacts/w9-rank2-receivers-20261029/provenance.json`. No source scans are committed.
