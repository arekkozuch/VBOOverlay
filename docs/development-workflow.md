# Task delivery and local acceptance

The owner has authorized the coordinator to implement the ordered
[FlappedEar Telemetry Jira backlog](https://kozucharkadiusz.atlassian.net/browse/KAN).
There are 100 separate numbered tasks; the seven milestone epics are additional
containers. Task numbers define queue order. The product contract remains
[F00–F20](product-vision.md).

## One task at a time

1. Read the Jira acceptance criteria and dependencies. Inspect current `main`,
   open PRs and uncommitted work before selecting the next unblocked task.
2. Move that task into progress and state the intended result. Continue an
   existing implementation PR when one already covers the task. Preserve other
   working copies and their uncommitted changes.
3. Implement only that task, using focused commits and updating relevant docs.
   Run applicable local build/tests. If the coordinator lacks the native
   toolchain, record that limitation and obtain build/test evidence through CI.
4. Push a PR referencing the Jira key. Review its actual diff and results; fix
   failed checks without removing required coverage or weakening acceptance.
5. Require Native CI on the final PR head: macOS arm64 and Windows x64, Debug
   and Release, on the configured Qt version. Verify every required job and its
   test/package steps rather than relying on an earlier successful run.
6. Merge only the verified head under the owner's development authorization.
   Recheck `main` and require its push-triggered Native CI to pass as well.
   If the head changes, reconcile the change and verify that exact source state.
7. Record the PR, implementation SHA, merge SHA, PR/main CI links, executed
   results and limitations in Jira. Mark the task done only after its criteria
   and the integration gate pass. Then take the next task.

Keep one implementation task active. If blocked, document the concrete blocker
before selecting another unblocked task; never mark blocked acceptance as done.
Give the owner progress updates and a short completed-task handoff, including
the revision available for local compilation. All Jira titles, descriptions,
acceptance criteria and updates must be in English.

Hosted CI exercises synthetic tests and internal package checks. Physical Mac,
private recordings, hardware encoders and interactive acceptance are separate
evidence. When a task explicitly requires them, it remains open until those
criteria are met. Development merge authorization does not authorize public
releases, tags or distribution. Work does not continue between conversation
turns unless a separately configured execution mechanism is actually running.

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
   toolchain rather than assuming CI's installation path.
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
open "build-native/native/FlappedEar Telemetry.app"
```

Run the build and tests between configuration and launch. If a different Qt
installation or generator is already configured, follow the repository's build
instructions for that environment and report the actual commands used.
