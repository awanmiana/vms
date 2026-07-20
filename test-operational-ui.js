const assert = require("assert");
const fs = require("fs");
const path = require("path");

const root = __dirname;
const html = fs.readFileSync(path.join(root, "index.html"), "utf8");
const css = fs.readFileSync(path.join(root, "styles.css"), "utf8");
const app = fs.readFileSync(path.join(root, "app.js"), "utf8");

function section(id, nextId) {
  const start = html.indexOf(`id="${id}"`);
  const end = nextId ? html.indexOf(`id="${nextId}"`, start) : html.length;
  assert(start >= 0, `expected #${id}`);
  assert(end > start, `expected #${nextId} after #${id}`);
  return html.slice(start, end);
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

run("Live and Playback use the shared layered camera workspace contract", () => {
  const live = section("operatorView", "devicesView");
  const playback = section("playbackView", "incidentsView");
  [live, playback].forEach((markup) => {
    assert.match(markup, /video-workspace-view/);
    assert.match(markup, /video-workspace-panel/);
    assert.match(markup, /video-resource-panel is-overlay-open/);
    assert.match(markup, /video-inspector-panel is-overlay-open/);
    assert.match(markup, /panel-toggle-btn/);
  });
  assert.match(playback, /id="playbackDrawer"/);
});

run("Playback keeps its date range, transport, draggable timeline, and footage legend", () => {
  const playback = section("playbackView", "incidentsView");
  const scrubberStart = playback.indexOf('id="pbScrubber"');
  const rangeStart = playback.indexOf('id="pbRangeBtn"');
  assert(scrubberStart >= 0);
  assert(rangeStart > scrubberStart, "range selector should be layered within the playback scrubber");
  [
    'id="pbRangeStartDate"',
    'id="pbRangeEndDate"',
    'id="pbPlayBtn"',
    'id="pbTrack"',
    'id="pbTrackAvailable"',
    "pb-footage-legend",
    'role="slider"'
  ].forEach((fragment) => assert(playback.includes(fragment), `missing ${fragment}`));
});

run("Operational CSS removes fixed viewport sizing and includes responsive layers", () => {
  const sharedContract = css.indexOf("Shared camera-first operational canvas");
  assert(sharedContract >= 0);
  const responsiveCss = css.slice(sharedContract);
  assert.match(responsiveCss, /html,\s*body\s*\{[\s\S]*?min-width:\s*0/);
  assert.match(responsiveCss, /\.video-workspace-panel #playbackSpatialCanvas/);
  assert.match(responsiveCss, /\.video-workspace-view\.active[\s\S]*?grid-template-columns:\s*minmax\(0,\s*1fr\)/);
  assert.match(responsiveCss, /@media \(max-width:\s*980px\)/);
  assert.match(responsiveCss, /@media \(max-width:\s*700px\)/);
  assert.match(responsiveCss, /\.playback-workspace-view \.pb-transport[\s\S]*?flex-wrap:\s*wrap/);
});

run("Search and panel behavior is routed by the active operational view", () => {
  assert.match(app, /classList\.toggle\("video-workspace-mode", isVideoWorkspace\)/);
  assert.match(app, /if \(state\.view === "playback"\)[\s\S]*?state\.playbackQuery = event\.target\.value/);
  assert.match(app, /toggleOperationalPanel\("live", "resource"\)/);
  assert.match(app, /toggleOperationalPanel\("playback", "inspector"\)/);
  assert.match(app, /function renderPlaybackDrawer\(\)/);
});

if (process.exitCode) {
  console.error("\nOperational UI tests failed.");
} else {
  console.log("\nAll operational UI tests passed.");
}
