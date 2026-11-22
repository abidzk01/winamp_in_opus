# Rule: English only for project information

Everything written **for or about this project** is in English:

- commit messages (subject, body, footer), tag messages and tag names
- branch names, pull request / merge request titles and descriptions
- code comments, identifier names, and added string literals
- documentation: `AGENTS.md`, `.opencode/rules/*`, `.opencode/skills/*`,
  `README.md`, `in_opus.txt`, changelog entries
- issue reports, release notes, code review comments, and any status or summary
  text produced about the project

Rules:

1. Never mix languages in one commit, document, or comment block.
2. New code and every changed line of code gets English comments, even when
   neighboring legacy comments are not English. Do not rewrite comments you did
   not need to touch.
3. Keep identifiers, config keys (`USE_REPLAY_GAIN`, `TARGET_LUFS`, …) and user
   visible messages in English; the plugin is English-only today.
4. Only the assistant's conversational reply to the user follows the user's
   language (Indonesian for this user). Any text that lands in the repository or
   is shared about the project stays English.
5. If you notice non-English text in the project, translate only the part you are
   already editing unless the user asks for a full translation.
