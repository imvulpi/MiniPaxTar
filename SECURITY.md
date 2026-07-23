# Security Policy for minipaxtar

The `minipaxtar` library is built with a security-first, defensive design mindset. Because archive parsing is a common vector for untrusted input attacks (such as path traversal, buffer overflows, and infinite loops), we take security reports very seriously.

---

## Supported Versions

Only the latest release on the `main` branch receives security updates. We encourage all users to stay updated with the latest version.

| Version / Branch | Supported          |
| :--- | :--- |
| `main` (Latest) | :white_check_mark: |
| Older releases | :x: |

---

## Core Security Objectives

When contributing or reporting issues, keep in mind the core safety guarantees `minipaxtar` strives to maintain:

* **Strict Bounds Checking:** Prevention of out-of-bounds reads or writes when processing malformed or truncated TAR/PAX headers.
* **Safe Integer Operations:** Prevention of integer wrap-around or overflow during archive size calculations.
* **Resource Exhaustion Prevention:** Safe bounds on extended PAX header parser loops to avoid infinite loops or memory exhaustion on invalid inputs.

---

## Reporting a Vulnerability

**Please do not report security vulnerabilities through public GitHub issues.**

If you discover a potential security flaw or vulnerability in `minipaxtar`:

1. **Private Report:** Open a [Draft Security Advisory](https://github.com/imvulpi/MiniPaxTar/security/advisories/new) on GitHub, or send a private email to the maintainer via the private GitHub email specified in `minipaxtar.h`.
2. **Details to Include:**
   * A brief description of the vulnerability and its potential impact.
   * Step-by-step instructions to reproduce the issue (or a proof-of-concept malformed archive file).
   * Any relevant code locations or compiler details.

---

## Response Process

* **Acknowledgement:** You will receive an initial response acknowledging your report.
* **Assessment:** The report will be investigated, verified, and assigned a severity rating.
* **Fix & Release:** A patch will be developed and merged into the `main` branch. A security advisory and credit will be issued upon release, unless you prefer to remain anonymous.

---

## Disclosure Policy

We follow a policy of **responsible disclosure**. We ask that you give us reasonable time to investigate and patch a reported vulnerability before making any details public.

Thank you for helping keep `minipaxtar` safe and reliable.
