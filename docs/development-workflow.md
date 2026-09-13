# Task delivery and local acceptance

## Current owner direction — 13 September 2026

Cloud CI is paused because the account has reached its quota. The workflow has
no push or pull-request triggers and its manual job is disabled. Do not dispatch,
rerun or re-enable cloud jobs without explicit owner authorization. This
supersedes earlier PR/main CI requirements in Jira and historical delivery notes.
Tasks 001–015 retain their recorded CI evidence; it is not evidence for new code.
Windows builds, tests and packaging remain paused. Development targets macOS.

## One local Codex instruction per Jira task

The coordinator prepares one self-contained English implementation prompt for the
next requested task in the ordered 100-task Jira backlog. The local Codex session
implements and verifies it on the owner's Mac. The prompt must contain:

- Jira key, goal, acceptance criteria and dependencies checked against current code.
- Relevant source paths, bounded scope and the applicable AGENTS.md invariants.
- A safe starting point: inspect branch/status/history and preserve unrelated work.
- Implementation, regression coverage and documentation requirements.
- Exact applicable local configure/build/test commands and an interactive scenario.
- A focused local commit and a report of SHA, commands, results, skips and limitations.

Do not replace implementation instructions with an instruction to start Cloud CI.
Do not claim completion from a prompt or unexecuted commands. If the local native
toolchain is unavailable, report what is missing and leave validation outstanding.

Keep one implementation task active. Review the local Codex result and actual
changes before updating Jira. A code task is complete only when its acceptance
criteria and applicable local build/tests pass. Run Debug application/tests as
the default local gate; also validate Release and deployed startup for packaging,
release-sensitive changes or when the task requires them. Keep private-media,
physical hardware/encoder and interactive checks separate from synthetic tests.

Local Codex must not push, open a PR, merge or publish unless the owner explicitly
requests it. When publication is authorized, use a focused PR and record local
verification for its exact head. After integration, verify the resulting main
revision locally; never substitute old CI results or silently enable Cloud CI.
All Jira content remains in English. Record the implementation SHA, local results,
limitations and any authorized PR/merge links. No public releases or tags are
implied. Historical CI wording elsewhere does not override this workflow.

## Local Codex: update, compile and test current main

When the owner asks the local Codex session to update, build and test the current
version:

1. Read `AGENTS.md` and inspect `git status --short --branch`. Preserve dirty
   work, local commits and the current branch. Do not reset, clean or stash work
   automatically. Use a separate checkout if the current worktree cannot be
   safely updated.
2. Fetch `origin` and identify `origin/main`. In a clean checkout already on
   `main`, use a fast-forward-only update. If local history has diverged, report
   it instead of rewriting it. Do not push from the local session unless asked.
3. Record `git rev-parse HEAD`, Qt and compiler versions. Configure using the
   existing [build instructions](../README.md#build); preserve a valid local
   toolchain rather than assuming a hosted runner's installation path.
4. Run the application build and test suite:

   ```bash
   cmake --build build-native --parallel
   ctest --test-dir build-native --output-on-failure
   ```

5. Launch the built application for the owner. Report the tested SHA, commands,
   results and skipped cases. Use the task's acceptance scenario for interactive
   checks; only use real recordings the owner has made available locally.

For the documented Apple Silicon/Homebrew setup, configuration and launch are:

```bash
cmake -S . -B build-native \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
open "build-native/native/Flapped Ear Telemetry.app"
```

Run the build and tests between configuration and launch. If a different Qt
installation or generator is already configured, follow the repository's build
instructions for that environment and report the actual commands used.
