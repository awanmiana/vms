const assert = require("assert");
const {
  MediaCapabilityError,
  SUPPORT_STATUSES,
  createStandardsRegistry
} = require("./shared/media-capabilities");

let failures = 0;

function run(name, test) {
  try {
    test();
    console.log(`ok - ${name}`);
  } catch (error) {
    failures += 1;
    console.error(`FAIL - ${name}`);
    console.error(error);
  }
}

function evidence(probe = "test-probe") {
  return {
    provider: "vms-test-runtime",
    probe,
    testedAt: "2026-08-09T00:00:00Z",
    version: "1"
  };
}

run("the standards catalog resolves canonical ids and aliases", () => {
  const registry = createStandardsRegistry();
  assert.strictEqual(registry.resolve("video-codec", "avc").id, "h264");
  assert.strictEqual(registry.resolve("video-codec", "hevc").id, "h265");
  assert.strictEqual(registry.resolve("image-format", "jpg").id, "jpeg");
  assert.ok(registry.list("protocol").some((item) => item.id === "rtsp-2"));
  assert.ok(registry.list("profile").some((item) => item.id === "onvif-t"));
});

run("catalog presence never implies runtime support", () => {
  const registry = createStandardsRegistry();
  assert.deepStrictEqual(
    registry.support("video-codec", "h264", "decode").status,
    SUPPORT_STATUSES.UNVERIFIED
  );
  assert.deepStrictEqual(
    registry.support("video-codec", "unknown-vendor-codec", "decode").reasonCode,
    "CAPABILITY_NOT_REGISTERED"
  );
});

run("supported reports require dated probe or conformance evidence", () => {
  const registry = createStandardsRegistry();
  assert.throws(
    () => registry.report({ kind: "video-codec", id: "h264", operation: "decode", status: "supported" }),
    (error) => error instanceof MediaCapabilityError && error.code === "SUPPORTED_EVIDENCE_REQUIRED"
  );
  const report = registry.report({
    kind: "video-codec",
    id: "avc",
    operation: "decode",
    status: "supported",
    evidence: evidence("gstreamer-decoder-probe")
  });
  assert.strictEqual(report.id, "h264");
  assert.strictEqual(report.evidence.testedAt, "2026-08-09T00:00:00.000Z");
});

run("negotiation selects only a mutually supported verified candidate", () => {
  const registry = createStandardsRegistry();
  registry.report({
    kind: "video-codec",
    id: "h264",
    operation: "decode",
    status: "supported",
    evidence: evidence()
  });
  const selected = registry.negotiate({
    kind: "video-codec",
    operation: "decode",
    candidates: [
      { id: "h265", status: "supported" },
      { id: "h264", status: "supported" }
    ]
  });
  assert.strictEqual(selected.status, "selected");
  assert.strictEqual(selected.selectedId, "h264");
  assert.strictEqual(selected.considered[0].localStatus, "unverified");

  const noMatch = registry.negotiate({
    kind: "video-codec",
    operation: "encode",
    candidates: ["h264", "h265"]
  });
  assert.strictEqual(noMatch.status, "no-match");
  assert.strictEqual(noMatch.reasonCode, "NO_VERIFIED_CAPABILITY_INTERSECTION");
});

run("vendor and future formats can be added without an unsafe fallback", () => {
  const registry = createStandardsRegistry();
  registry.register({
    kind: "video-codec",
    id: "x-example-vision",
    name: "Example extension codec",
    extension: true,
    standards: []
  });
  assert.strictEqual(registry.resolve("video-codec", "x-example-vision").extension, true);
  assert.strictEqual(registry.support("video-codec", "x-example-vision", "decode").status, "unverified");
  assert.strictEqual(registry.resolve("video-codec", "not-registered"), null);
});

if (failures) {
  console.error(`\n${failures} media capability test(s) failed.`);
  process.exitCode = 1;
} else {
  console.log("\nAll media capability tests passed.");
}
