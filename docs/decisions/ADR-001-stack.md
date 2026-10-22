# ADR-001: MVP stack and ownership

Status: **PROPOSED / pending Phase 1 evidence**. Owner tasks: P1-02 through P1-05.

## Context

We need independent camera and UI packages with a shared tested runtime. Existing movement, HUD, grid-inventory and minimap projects cover significant pieces, but feature names do not guarantee compatible behavior, APIs or permissions.

## Questions to resolve

1. Which executable/storefront and Windows or Proton host can the owner actually test?
2. Which one component owns camera transforms?
3. Does a preset/adapter meet C1–C5, or is a small custom controller required?
4. Which UI rendering path supports real native item actions and controller focus?
5. Which provider versions produce a working minimap and safe compass handoff?
6. Which follower/hotbar pieces can be configured or adapted without importing unwanted mechanics?
7. What source/art obligations apply to the exact chosen material?

## Selection criteria

Prefer the smallest maintainable route that passes the spikes, preserves scope, supports the pinned runtime and has a viable distribution path. Weight game evidence above theoretical flexibility. Reject a route that requires rebuilding broad systems to ship a small widget.

## Decision

Pending. Record selected/rejected options, exact versions, evidence and consequences here. No dependency choice is final merely because it appears in the planning candidate matrix.

## Consequences and follow-up

Pending. Update ARCHITECTURE, dependency-lock, runtime-matrix, phase tickets and any affected product assumption after the decision. Material changes to weight mechanics or required MVP features need a recorded owner choice.
