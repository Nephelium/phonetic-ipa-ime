# Phonetic IPA prototype

This directory contains prototype build/launch tools, frozen IPA resources,
source attribution and verification notes. C++ changes stay in the existing
engine/server/ui/windows boundaries and follow their local AGENTS.md files.

Use `scripts/prepare-resources.py`, `scripts/build-prototype.ps1` and
`scripts/stage-prototype.py` in that order. Never commit output/, tools/,
active.cin, learned databases, local configuration, credentials or input logs.
Resource updates require provenance, licenses and SHA-256 updates together.
Source/build checks, offscreen controls and actual TSF host input are separate
verification stages. Register-Prototype.ps1 changes system registration and is
an explicit user-run step, not part of build or tests.
