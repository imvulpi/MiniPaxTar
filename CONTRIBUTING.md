# Contributing to minipaxtar

Thank you for your interest in contributing to **minipaxtar**.

`minipaxtar` is designed to be a safe, minimal, zero-dependency C library for reading and writing POSIX PAX and USTAR tar archives. We welcome bug fixes, performance improvements, documentation updates, and tooling enhancements.

---

## Branch Naming Convention

All work should be done in feature or fix branches using the following format:

`<username>/<short-description>`

* Use lowercase
* Use hyphens for separation
* Keep the description short and focused

**Examples:**
* `imvulpi/fix-checksum-loop-warning`
* `dev-name/add-cmake-install-target`

---

## Commit Message Convention

`minipaxtar` uses a lightweight, conventional commit format:

`<type>: <short summary>`

### Commit Types
* **`feat`** - New functionality or capability
* **`fix`** - Bug fixes
* **`refactor`** - Code changes without behavior changes
* **`docs`** - Documentation-only changes
* **`perf`** - Performance improvements
* **`test`** - Adding or updating tests
* **`chore`** - Maintenance, CI, or tooling changes

### Examples
* `feat: add custom memory hook configuration`
* `refactor: optimize header checksum calculation`
* `fix: correct bounds check on extended PAX header parser`
* `docs: add compile-time define reference`
* `perf: eliminate redundant zero-copy offset seeks`

### Rules
* Use present tense ("add", not "added")
* Keep the summary under 72 characters
* One logical change per commit
* Avoid implementation details in the title

---

## Pull Requests

Before opening a pull request:

* Ensure your branch follows the naming convention.
* Ensure commits follow the commit message convention.
* Verify that core changes compile cleanly with `-DMPTAR_NO_STD` and strict C89 flags (`-Wall -Wextra -Werror -std=c89`).
* Ensure all tests pass.
* Keep pull requests focused and scoped. Tag your PR with `core` or `tooling` appropriately.

---

## Final Notes

`minipaxtar` values safety over cleverness, and predictability over abstraction. When in doubt, choose the simpler, safer, and more explicit solution.
