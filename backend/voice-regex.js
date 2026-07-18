const { createCommand, normalizeLookup } = require("./commands");

const FILLER_WORDS = /\b(um|uh|please|kindly|the|camera\s+number)\b/g;
const NUMBER_WORDS = new Map([
  ["zero", "0"],
  ["one", "1"],
  ["two", "2"],
  ["three", "3"],
  ["four", "4"],
  ["five", "5"],
  ["six", "6"],
  ["seven", "7"],
  ["eight", "8"],
  ["nine", "9"],
  ["ten", "10"],
  ["eleven", "11"],
  ["twelve", "12"]
]);

const VOICE_RULES = [
  { pattern: /^open group (.+)$/, intent: "OPEN_GROUP", targetType: "group" },
  { pattern: /^open (.+)$/, intent: "OPEN_CAMERA", targetType: "camera" },
  { pattern: /^show (.+)$/, intent: "OPEN_CAMERA", targetType: "camera" },
  { pattern: /^display (.+)$/, intent: "OPEN_CAMERA", targetType: "camera" },
  { pattern: /^close (all|everything)$/, intent: "STOP_ALL" },
  { pattern: /^close (.+)$/, intent: "CLOSE_CAMERA", targetType: "camera" },
  { pattern: /^(go to|jump to|pan to|take me to) (.+)$/, intent: "PAN_TO_ZONE", targetType: "zone", targetIndex: 2 },
  { pattern: /^start tracking (.+)$/, intent: "START_TRACKING", targetType: "camera" },
  { pattern: /^pause( session)?$/, intent: "PAUSE_SESSION" },
  { pattern: /^resume( session)?$/, intent: "RESUME_SESSION" },
  { pattern: /^escalate( session| case)?$/, intent: "ESCALATE_SESSION" }
];

class RegexCommandParser {
  constructor(aliasResolver = null) {
    this.aliasResolver = aliasResolver;
  }

  parse(transcript) {
    const clauses = normalizeTranscript(transcript)
      .split(/\s+(?:and|then)\s+/)
      .map((clause) => clause.trim())
      .filter(Boolean);

    return clauses
      .map((clause) => this.parseClause(clause, transcript))
      .filter(Boolean);
  }

  parseClause(clause, rawText) {
    for (const rule of VOICE_RULES) {
      const match = clause.match(rule.pattern);
      if (!match) continue;

      const targetName = rule.targetType ? match[rule.targetIndex || 1] : "";
      const target = rule.targetType ? this.resolveTarget(rule.targetType, targetName) : undefined;
      return createCommand({
        intent: rule.intent,
        target,
        source: "voice_regex",
        confidence: targetName && target && !target.id ? 0.72 : 0.9,
        rawText
      });
    }
    return null;
  }

  resolveTarget(type, name) {
    if (!name) return undefined;
    if (this.aliasResolver) {
      const resolved = this.aliasResolver(type, name);
      if (resolved) return resolved;
    }
    return { type, name };
  }
}

function normalizeTranscript(transcript) {
  return String(transcript || "")
    .toLowerCase()
    .replace(/[.,!?]/g, " ")
    .replace(FILLER_WORDS, " ")
    .split(/\s+/)
    .map((word) => NUMBER_WORDS.get(word) || word)
    .join(" ")
    .replace(/\bpull up\b/g, "open")
    .replace(/\bhide\b/g, "close")
    .replace(/\bstop\b/g, "close")
    .replace(/\s+/g, " ")
    .trim();
}

function createAliasResolver(db) {
  return (type, spokenName) => {
    const wanted = normalizeLookup(spokenName);
    const alias = db
      .table("voiceAliases")
      .find((item) => item.entityType === type && normalizeLookup(item.alias) === wanted);
    if (alias) return { type, id: alias.entityId, name: alias.alias };
    return null;
  };
}

module.exports = {
  RegexCommandParser,
  createAliasResolver,
  normalizeTranscript
};
