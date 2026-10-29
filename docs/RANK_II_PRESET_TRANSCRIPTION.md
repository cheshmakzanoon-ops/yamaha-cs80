# Rank-II preset matrix: first-pass source transcription

**Status: independent second review required; documentary draft only.** No audio, HardwareControl, factory patches, or executable source table is changed. This is schematic evidence, not measurement or proof of sonic equivalence.

Scope: 208 addresses (26 outputs × T53 B/E/F and T54 A–E). Existing reviewed T53 A/C/D columns are excluded. T54 uses printed columns A–E; F (96) has no drawn vertical branch. Conceptual bank positions G–K must not be used as schematic locators. Headers on both boards identify A–F as (91)–(96).

## Identity and interpretation

Names follow the foldout correspondence already recorded in [the circuit ledger](PRESET_CIRCUIT_LEDGER.md): T53 B=String 4, E=Bass, F=Clavichord 2; T54 A=Harpsichord 2, B=Organ 2, C=Guitar 2, D=Funky 2, E=Funky 4. The native T53/T54 pages themselves print column letters, not these tone names. Output/control labels follow the ledger's Panel 1 cross-check; numbers were traced against the right-edge native output labels.

- **O**: no branch drawn from this column to this output. Not a guaranteed zero bus voltage.
- **D**: diode-only branch, no drawn series resistor. Not a wire and not normalized1.
- **number k**: resistor in **kΩ**, in series with the branch diode. These tables contain no unqualified Ω entries.
- **U:** unresolved. Text after U is only visible-glyph evidence, never an accepted component configuration or resistor value.
- Every cell remains first-pass/unapproved even if readable. Crossed/triangle components remain revision-applicability unknown irrespective of how readable their glyphs are.

Both pages say CS-80 S/#1001~. T53 drawing ID KEC-4447-67; T54 KEC-4450-67. No service-update applicability has been guessed.

## T53 remaining columns

| Output | Control | B String 4 | E Bass | F Clavichord 2 |
|---|---|---|---|---|
| 1 | Speed | O | O | O |
| 2 | PWM | O | O | 110k |
| 3 | PW | O | 7.5k | D |
| 4 | Square switch | O | D | D |
| 5 | Saw switch | D | O | D |
| 6 | Noise | O | O | O |
| 7 | HPF | 27k | O | O |
| 8 | RESH | D | D | D |
| 9 | LPF | 1.6k | 12k | O |
| 10 | RESL | D | O | D |
| 11 | IL | 4.7k | 130k | O |
| 12 | AL | 30k | 3.3k | D |
| 13 | VCF attack | 8.2k | D | D |
| 14 | VCF decay | U:25k? glyph requires second look | 8.2k | 22k |
| 15 | VCF release | 3.3k | 1.6k | D |
| 16 | VCF level | D | D | D |
| 17 | Sine | O | O | O |
| 18 | VCA attack | 13k | 4.3k | D |
| 19 | VCA decay | D | 33k | 11k |
| 20 | VCA sustain | U:1.2k triangle | O | 43k |
| 21 | VCA release | 2.7k | D | 2k |
| 22 | VCA level | 1.8k | D | U:12k triangle |
| 23 | Initial brilliance | 22k | U:16k crossed;22k replacement glyph | 16k |
| 24 | Initial level | D | D | D |
| 25 | After brilliance | 13k | O | O |
| 26 | After level | D | U:crossed diode triangle | U:crossed diode triangle |

## T54 columns

| Output | Control | A Harpsichord 2 | B Organ 2 | C Guitar 2 | D Funky 2 | E Funky 4 |
|---|---|---|---|---|---|---|
| 1 | Speed | O | O | O | O | O |
| 2 | PWM | O | O | O | O | O |
| 3 | PW | 7.5k | O | O | O | O |
| 4 | Square switch | D | D | O | O | O |
| 5 | Saw switch | O | O | D | D | D |
| 6 | Noise | O | O | O | O | O |
| 7 | HPF | 2.7k | O | 22k | 7.5k | 6.8k |
| 8 | RESH | D | D | O | 5.6k | O |
| 9 | LPF | 1.8k | 6.2k | 18k | 4.3k | 6.2k |
| 10 | RESL | D | O | O | 18k | 51k |
| 11 | IL | O | O | O | 3.9k | O |
| 12 | AL | O | O | 2.4k | 4.7k | D |
| 13 | VCF attack | D | D | D | 16k | 11k |
| 14 | VCF decay | D | D | 36k | 8.2k | 15k |
| 15 | VCF release | D | D | 3.9k | D | D |
| 16 | VCF level | D | D | D | D | D |
| 17 | Sine | O | O | 6.8k | O | D |
| 18 | VCA attack | D | D | D | 3.6k | D |
| 19 | VCA decay | 16k | 5.6k | 82k | 13k | 11k |
| 20 | VCA sustain | O | 39k | 330k | 33k | 12k |
| 21 | VCA release | D | D | 2.7k | D | D |
| 22 | VCA level | D | D | U:8.2k crossed;12k replacement glyph | 4.7k | D |
| 23 | Initial brilliance | O | O | 22k | 8.2k | 12k |
| 24 | Initial level | D | D | D | D | D |
| 25 | After brilliance | O | O | U:56k crossed and crossed diode triangle | O | 5.6k |
| 26 | After level | U:crossed diode triangle | D | U:crossed diode triangle | D | D |

## Revision and topology gates

T53 branch-specific exclusions: B20 (triangle,1.2K glyph); F22 (triangle,12K glyph); E23 (triangle, crossed16K /22K replacement-looking glyph); E26/F26 (triangle and crossed/overdrawn diode). Additionally B14 is conservatively unresolved because the first reader sees25K but wants an independent glyph check; this is a readability gate, not an inferred service change.

T54 branch-specific exclusions: C22 (triangle, crossed8.2K /12K replacement-looking glyph); C25 (crossed56K-looking resistor text and crossed diode with triangle); A26/C26 (crossed diode with triangle). These crossed glyph readings are not authoritative replacement instructions.

**T54 also has crossed/triangle-marked primed output terminals throughout1′–26′.** This is a board-wide terminal/revision warning, separate from readable branch contents leading to the unprimed output buses. Do not claim the primed routing is known, implemented, deleted, or inapplicable to a selected serial number. No drawn local10k shunts are visible on T54; do not duplicate companion-board shunts or assume receiver impedance. T53 output rows4/5 show no local10k shunt; other rows show10k shunts to E. These observations do not solve the full control network.

## Provenance

Source inspected from native, non-resized PNG crops in GitHub Actions [run40790769942](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40790769942), T53 job123175135110 and T54 job123175135114. All four useful tiles for each page were decoded from CS80_CROP_DATA and viewed at original detail. SHA256 metadata below is preserved from the inspection workflow; no independent image hashing tool was available in this reader's connector-only environment.

### T53

Source: [original scan](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/35%20-%20T53%20Circuit%20Diagram.jpg). 5100×3300; 1206481 bytes.

Source SHA256: `204b97b6b408bfff051f978dfc9cc0cd70933b8ac80c941e6440f10325c19443`.

| Crop | Native source box | PNG SHA256 |
|---|---|---|
| t53-native-r0-c1 | 2136,0,4464,2264 | `0cdc2b30a12ed70334c49169e73662aaeeebde636b575d008b98e79579c824ad` |
| t53-native-r0-c2 | 4336,0,5100,2264 | `b39c3c863d05836976ecbbeb25511d78b99160fcb621f9ef31c95fb37316c6e7` |
| t53-native-r1-c1 | 2136,2136,4464,3300 | `64540888ccb299995fdef132e93a85b0656445c0ee04f086ffea11795e6a0230` |
| t53-native-r1-c2 | 4336,2136,5100,3300 | `92e72ceed09e859fe29d71d287303279120871083cf5d475af67faac7182109a` |

### T54

Source: [original scan](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/37%20-%20T54%20Circuit%20Diagram.jpg). 5100×3300; 1087235 bytes.

Source SHA256: `fd149fd9b1ca9be5fb39d04d3d0bb2cf6c6ecc95834c755c778511536dfac227`.

| Crop | Native source box | PNG SHA256 |
|---|---|---|
| t54-native-r0-c1 | 2136,0,4464,2264 | `e319619176e391274a00b2bd6c7427b11bb7726af7d23c7242f01fb3343f4051` |
| t54-native-r0-c2 | 4336,0,5100,2264 | `2703706d60d70235955820f2fa476a41fa1a3f05a6a239cba95ffe5872642280` |
| t54-native-r1-c1 | 2136,2136,4464,3300 | `72c490ec3332631965bcc9390b5cfaa58b82b792af0a123b1fa8f6c9eefecf0d` |
| t54-native-r1-c2 | 4336,2136,5100,3300 | `8b64e90537182edbe7bbf3b41c9a7244cae5728e7a4114e914c93dedc32713fb` |

## Integration boundary

The unreferenced document commit is based on20b3f16f2bca8b9b4016ee5db8380b741b62fc3e (tree e4bb2361c27dae574d328bff0b638da33bf2b8bb). Integrators should overlay only this new document onto the current main tree, never roll back newer files. Before any source-table integration, another reader must independently inspect all208 addresses and resolve discrepancies, while keeping all revision applicability unknown unless supported by separate service evidence. Production factory sounds additionally require selector/source impedance, diode operating points, downstream loading, routing, IC conversion and physical validation.
