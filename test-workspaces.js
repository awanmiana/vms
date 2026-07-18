const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const storage = new Map();
class FixedDate extends Date {
  static now() {
    return 1784332800000;
  }
}

const context = {
  __VMS_TEST_MODE__: true,
  console,
  Date: FixedDate,
  navigator: {
    hardwareConcurrency: 8,
    deviceMemory: 8
  },
  localStorage: {
    getItem(key) {
      return storage.has(key) ? storage.get(key) : null;
    },
    setItem(key, value) {
      storage.set(key, String(value));
    },
    removeItem(key) {
      storage.delete(key);
    }
  },
  window: {
    location: { hash: "" },
    history: { replaceState() {}, pushState() {} }
  },
  document: {
    getElementById() {
      return null;
    },
    querySelector() {
      return null;
    },
    querySelectorAll() {
      return [];
    }
  }
};
context.globalThis = context;

vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(__dirname, "app.js"), "utf8"), context, { filename: "app.js" });

const api = context.__VMS_TEST_API__;
assert(api, "app.js should expose the VMS test API");

const requiredHelpers = [
  "normalizeWorkspaceList",
  "activeWorkspace",
  "syncActiveWorkspaceFromLegacy",
  "loadWorkspaceIntoLegacy",
  "effectiveResourceLimit",
  "totalOpenCameraSessions",
  "workspaceAdmissionIds",
  "canAdmitCamera",
  "enforceGlobalCameraLimit",
  "nextWorkspaceId",
  "refreshCameraIndexes"
];
requiredHelpers.forEach((name) => {
  assert.strictEqual(typeof api[name], "function", `test API should expose ${name}`);
});

function plain(value) {
  return JSON.parse(JSON.stringify(value));
}

function camera(id) {
  return {
    id,
    name: `Camera ${id}`,
    area: "Test",
    floor: "Ground",
    direction: "North",
    deviceId: "",
    nvr: "Unassigned",
    channel: 1,
    status: "unknown",
    tags: [],
    related: [],
    previous: [],
    next: []
  };
}

function workspace(kind, id, openIds = [], overrides = {}) {
  return {
    id,
    name: overrides.name || `${kind === "live" ? "Live" : "Playback"} ${id}`,
    openIds: [...openIds],
    selectedId: overrides.selectedId ?? openIds[0] ?? "",
    gridDivision: overrides.gridDivision || (kind === "live" ? 9 : 4),
    rangeStart: kind === "playback" ? (overrides.rangeStart || "2026-07-18 00:00:00") : undefined,
    rangeEnd: kind === "playback" ? (overrides.rangeEnd || "2026-07-18 23:59:59") : undefined,
    floating: overrides.floating === true,
    layer: overrides.layer === "back" ? "back" : "front"
  };
}

function resetWorkspaceState({
  live = [workspace("live", "live-1")],
  playback = [workspace("playback", "playback-1")],
  activeLiveId = live[0].id,
  activePlaybackId = playback[0].id,
  limit = 8,
  view = "operator"
} = {}) {
  api.state.devices = [];
  api.state.cameras = ["A", "B", "C", "D", "E", "F", "G", "H"].map(camera);
  api.state.groups = [];
  api.state.liveWorkspaces = live;
  api.state.playbackWorkspaces = playback;
  api.state.activeLiveWorkspaceId = activeLiveId;
  api.state.activePlaybackWorkspaceId = activePlaybackId;
  api.state.resourceOptimizationMode = "manual";
  api.state.maxActiveStreams = limit;
  api.state.view = view;
  api.loadWorkspaceIntoLegacy("live");
  api.loadWorkspaceIntoLegacy("playback");
  api.refreshCameraIndexes();
}

function run(name, fn) {
  try {
    fn();
    console.log(`ok - ${name}`);
  } catch (error) {
    console.error(`FAIL - ${name}`);
    console.error(error);
    process.exitCode = 1;
  }
}

run("active compatibility mirrors round-trip independent Live and Playback workspaces", () => {
  resetWorkspaceState({
    live: [
      workspace("live", "live-1", ["A"], { selectedId: "A", gridDivision: 4 }),
      workspace("live", "live-2", ["B", "C"], { selectedId: "C", gridDivision: 9 })
    ],
    playback: [
      workspace("playback", "playback-1", ["D"], {
        selectedId: "D",
        rangeStart: "2026-07-17 01:00:00",
        rangeEnd: "2026-07-17 02:00:00"
      }),
      workspace("playback", "playback-2", ["E"], {
        selectedId: "E",
        gridDivision: 1,
        rangeStart: "2026-07-16 03:00:00",
        rangeEnd: "2026-07-16 04:00:00"
      })
    ]
  });

  api.state.openIds = ["A", "F"];
  api.state.selectedId = "F";
  api.state.gridDivision = 16;
  api.state.playbackOpenIds = ["D", "B"];
  api.state.playbackSelectedId = "B";
  api.state.playbackGridDivision = 4;
  api.state.playbackRangeStart = "2026-07-17 05:00:00";
  api.state.playbackRangeEnd = "2026-07-17 06:00:00";
  api.syncActiveWorkspaceFromLegacy("live");
  api.syncActiveWorkspaceFromLegacy("playback");

  assert.deepStrictEqual(plain(api.state.liveWorkspaces[0].openIds), ["A", "F"]);
  assert.strictEqual(api.state.liveWorkspaces[0].selectedId, "F");
  assert.strictEqual(api.state.liveWorkspaces[0].gridDivision, 16);
  assert.deepStrictEqual(plain(api.state.liveWorkspaces[1].openIds), ["B", "C"]);
  assert.deepStrictEqual(plain(api.state.playbackWorkspaces[0].openIds), ["D", "B"]);
  assert.strictEqual(api.state.playbackWorkspaces[0].rangeStart, "2026-07-17 05:00:00");
  assert.deepStrictEqual(plain(api.state.playbackWorkspaces[1].openIds), ["E"]);

  api.state.activeLiveWorkspaceId = "live-2";
  api.state.activePlaybackWorkspaceId = "playback-2";
  api.loadWorkspaceIntoLegacy("live");
  api.loadWorkspaceIntoLegacy("playback");

  assert.deepStrictEqual(plain(api.state.openIds), ["B", "C"]);
  assert.strictEqual(api.state.selectedId, "C");
  assert.strictEqual(api.state.gridDivision, 9);
  assert.deepStrictEqual(plain(api.state.playbackOpenIds), ["E"]);
  assert.strictEqual(api.state.playbackSelectedId, "E");
  assert.strictEqual(api.state.playbackRangeStart, "2026-07-16 03:00:00");
  assert.strictEqual(api.state.playbackRangeEnd, "2026-07-16 04:00:00");
});

run("workspace normalization deduplicates camera IDs and workspace IDs", () => {
  const normalized = api.normalizeWorkspaceList(
    "live",
    [
      { id: "duplicate", openIds: ["A", "A", "B"], gridDivision: 4 },
      { id: "duplicate", openIds: ["C", "C"], gridDivision: 4 },
      { id: "", openIds: ["D", "D"], gridDivision: 4 }
    ],
    workspace("live", "legacy")
  );

  assert.strictEqual(normalized.length, 3);
  assert.strictEqual(new Set(normalized.map((item) => item.id)).size, 3, "workspace IDs must be unique");
  assert.deepStrictEqual(plain(normalized.map((item) => item.openIds)), [["A", "B"], ["C"], ["D"]]);
});

run("shared manual cap covers mixed Live and Playback admission", () => {
  resetWorkspaceState({
    live: [
      workspace("live", "live-1", ["A", "B"]),
      workspace("live", "live-2", ["C"])
    ],
    playback: [
      workspace("playback", "playback-1", ["D"]),
      workspace("playback", "playback-2", ["E"])
    ],
    limit: 5
  });

  assert.strictEqual(api.effectiveResourceLimit(), 5);
  assert.strictEqual(api.totalOpenCameraSessions(), 5);
  assert.strictEqual(api.canAdmitCamera("live", "F"), false, "a new session must be rejected at the mixed cap");
  assert.strictEqual(api.canAdmitCamera("live", "A"), true, "an existing active-workspace camera consumes no new slot");

  api.state.playbackWorkspaces[1].openIds = [];
  assert.strictEqual(api.totalOpenCameraSessions(), 4);
  assert.strictEqual(api.canAdmitCamera("live", "F"), true);

  api.state.maxActiveStreams = 4;
  const replacement = api.workspaceAdmissionIds("live", ["E", "F", "A", "E"]);
  assert.deepStrictEqual(
    plain(replacement),
    ["E", "F"],
    "replacement admission should release the current workspace allocation and preserve request order"
  );
});

run("limit reduction is global, deterministic, and repairs overflow workspaces", () => {
  resetWorkspaceState({
    live: [
      workspace("live", "live-1", ["A", "B"], { selectedId: "B" }),
      workspace("live", "live-2", ["C"], { selectedId: "C" })
    ],
    playback: [
      workspace("playback", "playback-1", ["D"], { selectedId: "D" }),
      workspace("playback", "playback-2", ["E"], { selectedId: "E" })
    ],
    limit: 5,
    view: "playback"
  });

  api.state.maxActiveStreams = 3;
  api.enforceGlobalCameraLimit();

  assert.strictEqual(api.totalOpenCameraSessions(), 3);
  assert.deepStrictEqual(plain(api.activeWorkspace("playback").openIds), ["D"]);
  assert.deepStrictEqual(plain(api.activeWorkspace("live").openIds), ["A", "B"]);
  assert.deepStrictEqual(plain(api.state.liveWorkspaces[1].openIds), []);
  assert.deepStrictEqual(plain(api.state.playbackWorkspaces[1].openIds), []);
  assert.strictEqual(api.state.liveWorkspaces[1].selectedId, "");
  assert.strictEqual(api.state.playbackWorkspaces[1].selectedId, "");
  assert.match(api.state.resourceLimitNotice, /2 camera sessions closed/);
});

run("the same camera is deduplicated within a workspace but counted in every tab", () => {
  const normalizedLive = api.normalizeWorkspaceList(
    "live",
    [{ id: "live-1", openIds: ["A", "A", "A"] }],
    workspace("live", "legacy")
  );
  resetWorkspaceState({
    live: [
      normalizedLive[0],
      workspace("live", "live-2", ["A"])
    ],
    playback: [workspace("playback", "playback-1", ["A"])],
    limit: 3
  });

  assert.deepStrictEqual(plain(api.state.liveWorkspaces[0].openIds), ["A"]);
  assert.strictEqual(api.totalOpenCameraSessions(), 3, "each workspace placement is a distinct conservative session");
  assert.strictEqual(api.canAdmitCamera("live", "A"), true);
  assert.deepStrictEqual(plain(api.workspaceAdmissionIds("live", ["A", "A"])), ["A"]);
});

run("rapid workspace ID allocation stays unique even when Date.now is unchanged", () => {
  resetWorkspaceState();
  const ids = [];
  for (let index = 0; index < 100; index += 1) {
    const id = api.nextWorkspaceId("live");
    ids.push(id);
    api.state.liveWorkspaces.push(workspace("live", id));
  }
  assert.strictEqual(new Set(ids).size, ids.length);
  assert(ids.every((id) => id.startsWith("live-")));
});

if (process.exitCode) {
  console.error("\nWorkspace tests failed.");
} else {
  console.log("\nAll workspace tests passed.");
}
