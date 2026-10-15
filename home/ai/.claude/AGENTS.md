# Global Preferences

## Principles
- Be simple and clear.
- Prefer native language solutions. Use external packages or libraries only when clearly better than the native solution.
- Prioritize pure functions.
- Never assume. Every claim must come from a verifiable source.
- Base decisions on verifiable data when relevant.
- Nobody is always right (neither the user nor the agent). Both converge toward correctness through evidence.

## Tests
- Write only relevant tests. Do not repeat tests.
- When tests differ only by input and output, use parameterized tests (e.g., JUnit `@ParameterizedTest`).

## After Finishing a Task
- Remove old builds and unused packages installed during development.
- Record lessons learned in `NOTES.md` (global: `~/.claude/NOTES.md`, or local: project root). Purpose: avoid repeating mistakes.

## Scripts
- Persist any reusable script (database queries, counters, analyses, etc.) in `~/agent-scripts/`.
- Document each script in `~/agent-scripts/SCRIPTS.md`: what it does, when to use it, how to use it.
- Reference: `~/agent-scripts/SCRIPTS.md`

## Documentation
Write documentation following these principles:
- Objective: go straight to the point, without unnecessary introductions.
- Precise: use technical terms with consistent meaning; avoid ambiguity.
- Impersonal: describe the system; avoid personal opinions or experiences.
- Formal, without excess: use professional language, without overly elaborate constructions.
- Consistent: keep the same naming, structure, and formatting patterns.
- Instructive: explain what to do, how to do it, and what result to expect.

Example:
- Inadequate (informal and vague): `Now just configure the token and run the project. If something goes wrong, check that everything is correct.`
- Adequate (technical and objective): `Set the environment variable `API_TOKEN` to a valid token. Then run `npm run dev` to start the application. If an authentication failure occurs, verify the token validity and the associated permissions.`

## System Configuration
- OS: Arch Linux, kernel Linux 7.2.9-arch1-1
- Shell: bash 5.3.20 (`/usr/bin/bash`)
- Editor/IDE: Cursor 3.24.12. `EDITOR` and `VISUAL` are unset.
- Languages and versions: Python 3.14.8, Node.js 26.11.1, GCC 16.2.1, Lua 5.5.1, Perl 5.42.3
- Package managers: pacman 7.1.0 (libalpm 16.0.1), yay 13.0.1
- Other relevant tools: git 2.56.0, GNU Stow 2.4.1, Hyprland 0.56.2, kitty 0.49.2, ripgrep 15.2.0, Claude Desktop 2.37337.1
