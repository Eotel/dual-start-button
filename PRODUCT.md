# Product

## Register

product

## Users

Developers, maintainers, and operators working with local physical Dual Start Button devices during firmware flashing, BLE debugging, and fallback input validation. They are usually at a desk or test bench with USB-connected M5Stack-family devices, Chrome/Web Bluetooth, and sometimes a phone for browser or PWA validation. Their job is to connect generic devices, assign two logical slots, inspect state, and verify that button input can satisfy the start condition without fixed device roles.

## Product Purpose

Dual Start Button provides firmware, protocol tooling, and debug web surfaces for M5Stack-family ESP32 button devices. The system lets a host choose any two supported devices from a set, link them to slot 1 and slot 2, and derive a start condition from host-side active state. Success means the operator can flash or update devices, connect and reconnect them, replace devices, inspect state, and validate start behavior with minimal ambiguity.

## Brand Personality

Quiet, precise, recoverable. The interface should feel like a focused engineering instrument: dense enough for repeated debugging, restrained enough to scan quickly, and explicit about connection state, slot assignment, and limitations.

## Anti-references

It should not look like a marketing landing page, a decorative SaaS dashboard, a game controller skin, or a consumer pairing wizard that hides protocol truth. Avoid fixed A/B role language, ambiguous pairing/link wording, oversized cards, decorative gradients, side-stripe cards, heavy shadows, and UI copy that implies HID fallback has the same guarantees as BLE GATT.

## Design Principles

- Protocol truth before polish: show the actual state, identity, freshness, and limitations plainly.
- Generic devices stay generic: slot assignment is host-level and must never imply baked-in device roles.
- Recovery is part of the product: every mode should make replacement, reflash, and fallback paths understandable.
- Dense but calm: prioritize stable layout, readable labels, and fast repeated actions over decorative hierarchy.
- Separate capability tiers: distinguish canonical GATT behavior, browser HID fallback, and native or mobile-only capabilities.

## Accessibility & Inclusion

Target WCAG AA contrast for debug and operational UI. Preserve keyboard operability, visible focus, reduced-motion safety, and layouts that do not shift during live state updates. Use plain labels for state and errors so color is never the only signal.
