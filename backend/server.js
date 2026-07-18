const fs = require("fs");
const http = require("http");
const path = require("path");
const { URL } = require("url");
const { FileDatabase } = require("./file-db");
const { handleInventoryRoute, handleStubRoute } = require("./api-routes");

const DEFAULT_HOST = "127.0.0.1";
const DEFAULT_PORT = 5173;
const MAX_JSON_BYTES = 2 * 1024 * 1024;

const MIME_TYPES = {
  ".css": "text/css; charset=utf-8",
  ".gif": "image/gif",
  ".htm": "text/html; charset=utf-8",
  ".html": "text/html; charset=utf-8",
  ".ico": "image/x-icon",
  ".jpeg": "image/jpeg",
  ".jpg": "image/jpeg",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".png": "image/png",
  ".svg": "image/svg+xml; charset=utf-8",
  ".webp": "image/webp"
};

const PRIVATE_TOP_LEVEL_PATHS = new Set([".agents", ".codex", ".git", "backend"]);

function corsHeaders() {
  return {
    "Access-Control-Allow-Headers": "Content-Type",
    "Access-Control-Allow-Methods": "GET, HEAD, PUT, OPTIONS",
    "Access-Control-Allow-Origin": "*"
  };
}

function sendJson(response, status, body, extraHeaders = {}) {
  const payload = `${JSON.stringify(body)}\n`;
  response.writeHead(status, {
    ...corsHeaders(),
    "Cache-Control": "no-store",
    "Content-Length": Buffer.byteLength(payload),
    "Content-Type": "application/json; charset=utf-8",
    ...extraHeaders
  });
  response.end(payload);
}

function readJsonBody(request, maxBytes = MAX_JSON_BYTES) {
  return new Promise((resolve, reject) => {
    let length = 0;
    let exceeded = false;
    const chunks = [];

    request.on("data", (chunk) => {
      length += chunk.length;
      if (length > maxBytes) {
        exceeded = true;
        return;
      }
      chunks.push(chunk);
    });
    request.on("end", () => {
      if (exceeded) {
        const error = new Error(`JSON body exceeds the ${maxBytes}-byte limit.`);
        error.status = 413;
        reject(error);
        return;
      }

      const raw = Buffer.concat(chunks).toString("utf8").trim();
      if (!raw) {
        const error = new Error("A JSON request body is required.");
        error.status = 400;
        reject(error);
        return;
      }

      try {
        resolve(JSON.parse(raw));
      } catch {
        const error = new Error("Request body is not valid JSON.");
        error.status = 400;
        reject(error);
      }
    });
    request.on("error", reject);
  });
}

function safeStaticPath(staticRoot, pathname) {
  let decoded;
  try {
    decoded = decodeURIComponent(pathname);
  } catch {
    return null;
  }
  if (decoded.includes("\0")) return null;

  const relative = decoded === "/" ? "index.html" : decoded.replace(/^\/+/, "");
  const segments = relative.split(/[\\/]+/).filter(Boolean);
  if (!segments.length || PRIVATE_TOP_LEVEL_PATHS.has(segments[0].toLowerCase())) return null;

  const root = path.resolve(staticRoot);
  let candidate = path.resolve(root, ...segments);
  const rootPrefix = `${root}${path.sep}`;
  if (candidate !== root && !candidate.startsWith(rootPrefix)) return null;

  try {
    if (fs.statSync(candidate).isDirectory()) candidate = path.join(candidate, "index.html");
    if (!fs.statSync(candidate).isFile()) return null;
  } catch {
    return null;
  }

  return candidate;
}

function serveStatic(request, response, staticRoot, pathname) {
  if (request.method !== "GET" && request.method !== "HEAD") {
    sendJson(response, 405, { error: "Method Not Allowed" }, { Allow: "GET, HEAD" });
    return;
  }

  const filePath = safeStaticPath(staticRoot, pathname);
  if (!filePath) {
    sendJson(response, 404, { error: "Not Found", route: `${request.method} ${pathname}` });
    return;
  }

  const extension = path.extname(filePath).toLowerCase();
  const contentType = MIME_TYPES[extension];
  if (!contentType) {
    sendJson(response, 404, { error: "Not Found", route: `${request.method} ${pathname}` });
    return;
  }

  const stat = fs.statSync(filePath);
  response.writeHead(200, {
    "Cache-Control": extension === ".html" ? "no-cache" : "public, max-age=60",
    "Content-Length": stat.size,
    "Content-Type": contentType,
    "X-Content-Type-Options": "nosniff"
  });
  if (request.method === "HEAD") {
    response.end();
    return;
  }
  fs.createReadStream(filePath).pipe(response);
}

function createDatabase(filePath) {
  const db = new FileDatabase(filePath || undefined);
  db.load();
  return db;
}

function createVmsServer({ db, databasePath, staticRoot = path.join(__dirname, "..") } = {}) {
  const database = db || createDatabase(databasePath);

  return http.createServer(async (request, response) => {
    try {
      const requestUrl = new URL(request.url || "/", "http://localhost");
      const pathname = requestUrl.pathname;

      if (request.method === "OPTIONS") {
        response.writeHead(204, { ...corsHeaders(), "Content-Length": "0" });
        response.end();
        return;
      }

      if (pathname.startsWith("/api/")) {
        let body;
        if (pathname === "/api/inventory" && request.method === "PUT") {
          body = await readJsonBody(request);
        }

        const inventoryResponse = handleInventoryRoute(database, request.method, pathname, body);
        const routeResponse = inventoryResponse || handleStubRoute(request.method, pathname);
        sendJson(response, routeResponse.status, routeResponse.body, routeResponse.headers);
        return;
      }

      serveStatic(request, response, staticRoot, pathname);
    } catch (error) {
      sendJson(response, error.status || 500, {
        error: error.status ? "Invalid request" : "Internal Server Error",
        message: error.message
      });
    }
  });
}

function startServer({ host = process.env.HOST || DEFAULT_HOST, port = Number(process.env.PORT) || DEFAULT_PORT } = {}) {
  const server = createVmsServer({ databasePath: process.env.VMS_DB_PATH });
  server.listen(port, host, () => {
    const address = server.address();
    console.log(`VMS Operator Console: http://${host}:${address.port}`);
  });
  return server;
}

if (require.main === module) {
  startServer();
}

module.exports = {
  DEFAULT_HOST,
  DEFAULT_PORT,
  MAX_JSON_BYTES,
  createVmsServer,
  readJsonBody,
  safeStaticPath,
  startServer
};
