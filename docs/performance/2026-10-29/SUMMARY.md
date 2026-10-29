# Results summary

72 rows, 55296 measured callbacks. See METHODOLOGY.md and full results.csv.

## Isolation ON; ranges across the three scenarios

| Host Hz | Block | min range ms | median range ms | p99 range ms | worst max ms | median RTF range | budget exceedances / callbacks |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 44100 | 64 | 0.293–0.297 | 0.313–0.322 | 0.732–0.945 | 2.854 | 0.216–0.222 | 7 / 2304 |
| 44100 | 256 | 1.196–1.203 | 1.246–1.256 | 1.735–1.918 | 3.278 | 0.215–0.216 | 0 / 2304 |
| 44100 | 1024 | 4.806–4.859 | 5.153–5.377 | 7.308–9.737 | 15.133 | 0.222–0.232 | 0 / 2304 |
| 48000 | 64 | 0.291–0.295 | 0.299–0.308 | 0.372–0.610 | 1.408 | 0.224–0.231 | 1 / 2304 |
| 48000 | 256 | 1.186–1.203 | 1.243–1.287 | 1.618–2.194 | 7.458 | 0.233–0.241 | 2 / 2304 |
| 48000 | 1024 | 4.799–4.812 | 5.017–5.152 | 6.481–7.518 | 10.978 | 0.235–0.242 | 0 / 2304 |
| 96000 | 64 | 0.289–0.291 | 0.296–0.321 | 0.477–0.959 | 3.361 | 0.445–0.482 | 32 / 2304 |
| 96000 | 256 | 1.170–1.182 | 1.235–1.347 | 2.806–3.068 | 10.310 | 0.463–0.505 | 37 / 2304 |
| 96000 | 1024 | 4.728–4.757 | 5.048–5.156 | 6.468–8.074 | 14.997 | 0.473–0.483 | 2 / 2304 |
| 192000 | 64 | 0.285–0.287 | 0.300–0.304 | 0.528–0.837 | 4.358 | 0.901–0.912 | 529 / 2304 |
| 192000 | 256 | 1.151–1.165 | 1.195–1.265 | 1.505–3.931 | 6.295 | 0.896–0.948 | 468 / 2304 |
| 192000 | 1024 | 4.653–4.671 | 4.872–5.035 | 6.226–8.756 | 15.470 | 0.913–0.944 | 424 / 2304 |

## Computational ratios (not equal features)

| Host Hz | median on/off ratio range | median of pair ratios |
|---:|---:|---:|
| 44100 | 0.970–1.092 | 1.030 |
| 48000 | 1.019–1.078 | 1.048 |
| 96000 | 0.996–1.078 | 1.020 |
| 192000 | 1.016–1.084 | 1.052 |


Full per-case min/median/p95/p99/max, event statistics and checksums are in [results.csv](results.csv), with the repeated high-rate case in [confirm192.csv](confirm192.csv). Read [METHODOLOGY.md](METHODOLOGY.md) before interpreting these values. This is a contended cloud microbenchmark, not DAW or hard-realtime qualification.
