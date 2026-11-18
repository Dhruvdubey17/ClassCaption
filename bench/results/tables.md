| Pass | Model | Backend | WER |
|---|---|---|---|
| Live | ggml-large-v3-turbo-q5_0.bin | coreml | 10.4% |
| After class | ggml-large-v3-q5_0.bin | metal | 2.6% |

3 TED talks, 19.1 minutes. Source: `bench/results/accuracy.json`.

| Speech to caption | Median | p95 |
|---|---|---|
| First shown (draft) | 0.89 s | 1.67 s |
| Final | 2.01 s | 4.15 s |

793 words, 5.7 minutes played in real time. Source: `bench/results/latency.json`.
