# Threat model

## What this project does and where untrusted input enters
DDNet is an online game, consisting of a client and server.

Untrusted inputs include for example the following: network packets, map files, demo files, ghost files, line-based text files, json-based text files, images, sounds

## Components that matter most / least
- Matters most: DDNet-Server, DDNet client
- Lower: Various tools
- Out of scope: Tests

## How to exercise it
- src/build contains a regular build
- src/build-asan contains a build with AddressSanitizer and UndefinedBehaviorSanitizer

## How you rate severity
- Out of bounds writes, use-after-free, double free are high priority.
- If the overflow is controlled and can lead to RCE it's critical.
- Authentication bypass in server is critical.
- Server crashes caused by unauthenticated clients are critical.
- DoS is medium priority.

## Anything to leave alone
- Maps are known to contain a bunch of out of bounds reads.
- The `dbg_assert` and `dbg_assert_failed` statements are also compiled in release mode. Failed assertions are correctness instead of security issues, except for server crashes caused by (unauthenticated) clients. It is seldom correct to fix an issue simply by removing a `dbg_assert` or altering its condition.

## How reports and patches should look
Deterministic reproducer and sanitizer outputs are most useful. The patch should be minimal and fix the root cause, and follow docs/CONTRIBUTING.md.
