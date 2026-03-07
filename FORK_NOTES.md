# JK2MV Fork Notes

This fork starts with a maintenance-first strategy.

## Goals

- Keep the fork close to upstream so fixes can be merged forward easily.
- Improve maintainability before attempting larger engine changes.
- Relearn the codebase incrementally with small, reviewable changes.
- Preserve multiplayer and mod compatibility unless a change is explicitly marked as experimental.

## Initial rules

- Prefer small pull requests.
- Separate refactors from behavior changes.
- Document architectural findings as they are discovered.
- Touch build and tooling before touching critical runtime code.

## First milestone

M1 focuses on safe groundwork:

- clarify build and CI behavior,
- document targets and dependencies,
- identify low-risk subsystems for first code refactors,
- avoid protocol, VM, and renderer rewrites.
