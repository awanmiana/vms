# Deterministic voice command leg - P1-12 second slice, native pass

> Scope note for the native production runtime. Subordinate to
> `Development_plan.md`. Approved by the owner's 2026-08-09 directive to build
> the remaining listed native capabilities using worldwide standards.

## Decision

Follow the W3C Speech Recognition Grammar Specification (SRGS) grammar-
processor boundary without claiming that this recognizer-neutral component is
itself an SRGS parser: a recognizer supplies a language-tagged transcript and
confidence value; a finite, deterministic, domain grammar maps accepted
phrases to the same typed P1-12 command envelope used by QML, text, HTTP, and
later agents. Deployments may bind an SRGS-capable recognizer at that boundary.

This slice is deliberately recognizer-neutral. It does not select a cloud
speech provider, upload audio, identify speakers, or claim microphone support.
Those are deployment/privacy decisions. The product authority begins at the
recognition-result boundary.

## Included

- Pure C++17 `VoiceCommandMapper`, linked into `vms_command`.
- Explicit registered BCP-47 tags (`en`, `en-US`, and `en-GB`); every other
  tag fails closed until its own tested grammar is registered.
- Confidence threshold, ambiguity detection, normalized number words, and
  typed arguments for workspace, playback/replay, alarm, and bounded device
  phrases.
- `CommandController::runVoice`: accepted mappings invoke the existing command
  registry with source `voice`; refusals also traverse the registry audit sink.
- A dangerous command may be requested but voice never supplies `confirmed`;
  step-up confirmation remains a separate interaction.
- Pure self-tests for accepted phrases, typed values, unsupported language,
  low confidence, ambiguity, unmatched input, and the dangerous-action gate.

## Excluded

- Microphone capture, speech-to-text engine/provider selection, wake-word
  detection, speaker identity, biometric authentication, and audio retention.
- Fuzzy/NLU/LLM interpretation, guessed camera names, and multilingual claims.
- One-utterance confirmation of destructive or physical actions.

## Acceptance

1. The same transcript always produces the same command id and typed args.
2. Unsupported language, confidence below policy, ambiguity, and no-match
   produce no executable command.
3. A mapped command is still subject to validation, capability, confirmation,
   deterministic result mapping, and audit in `CommandRegistry`.
4. Spoken `confirm` cannot bypass the dangerous-action gate.
5. Existing command, workspace, API, coverage, and root regressions remain
   green.

## Standards references

- W3C Speech Recognition Grammar Specification 1.0:
  <https://www.w3.org/TR/speech-grammar/>
- IETF BCP 47 language tags (RFC 5646):
  <https://www.rfc-editor.org/rfc/rfc5646.html>
