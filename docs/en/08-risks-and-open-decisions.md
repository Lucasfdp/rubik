# Risk Register & Open Decisions

*(Part of the Rubik prep docs — see `00-overview.md` for the index and `GLOSSARY.md` for term definitions.)*

## Risk register

| Risk | Severity | Mitigation |
|---|---|---|
| Bonus work starts before the mandatory part is perfect → bonus graded as zero (R11) | **High** | The Sprint 4 freeze checklist (`05-roadmap-mandatory.md`) is a hard gate. Mandatory and bonus are separate binaries, so a bonus regression can't touch the mandatory one. |
| Cubie indexing disagreement between the two of you | **High** | Fixed together in Sprint 0, written into `DECISIONS.md`, never changed afterward. |
| Inadmissible pruning table (one that overestimates distance) → intermittent wrong answers | High | Property test: for random states, check `prune_value ≤ actual_solution_length` always holds. |
| Invalid cube input → search never terminates → R6 failure | High | Validate parity/orientation invariants *before* entering the search. No exceptions to this. |
| raylib won't build on cluster machines | Medium | 1-hour build spike in Sprint 5, **before** any render code is written; MLX fallback pre-agreed in advance. |
| Float drift in the 3D animation desyncs the render from the logical state | Medium | Commit-on-complete + reset the animation transform to identity after every move; the 1,000-move drift test in Sprint 6 catches this. |
| Table generation at startup pushes total runtime near the 3s limit | Low | Tables are under 10 MB and generate via BFS in well under a second — measure it directly in Sprint 3 anyway, don't just assume. |
| One of you can't defend the other's module | Medium | The Sprint 8 swap rehearsal; deliberate cross-pairing already built into Sprints 1–2 and 6. |

## Open decisions for the two of you

1. **Kociemba only, or Kociemba + Thistlethwaite?** (`02-algorithms.md` §3.C vs. §3.E). Recommendation: build the shared substrate generically, ship Kociemba as the core solver, and add Thistlethwaite in Sprint 7 only if you're ahead of schedule.
2. **raylib or MLX?** (`03-graphics.md` §4.A vs. §4.B). Recommendation: decide based on the Sprint 5 build-spike result, not by personal preference — the spike result is the more reliable signal.
3. **Who takes Dev A vs. Dev B?** The A track leans more toward I/O and correctness-plumbing; the B track leans more toward combinatorics and search. Swap roles between Sprint 2 and Sprint 6, so neither of you does the same flavour of work twice.
4. **One binary with a `-v` visual flag, or two separate binaries?** Recommendation: two — this protects the mandatory build from any bonus-side breakage (see `04-architecture.md`).
5. **Generate tables at startup, or cache them to disk?** Recommendation: at startup. It's fast enough to stay well inside the time budget, and it removes an awkward question at defence about whether cached tables count as pre-computed cheating.
