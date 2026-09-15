---
type: Rule
title: Validate inspector exports before publication
description: Encoder success, repeatability and decoded output must be checked independently of build success.
tags: [okf, inspector, testing, exports]
---

# Validate inspector exports before publication

**When** implementing inspector exports, **then** write to a unique temporary
file beside the destination, verify nonempty completed output, and replace the
destination only on success. The user explicitly expects repeated export
clicks to replace earlier dumps. A failed encoder must preserve the earlier
destination and report its error.

For PNG, honor the WIC negotiated pixel format with explicit channel
conversion. Decode the saved result to verify its dimensions; regression
tests must also verify colour/alpha fidelity and repeated writes. Compilation
alone did not expose the RGBA/BGRA negotiation failure that produced empty
files; the September 2026 Visual Studio capture demonstrated this failure.
