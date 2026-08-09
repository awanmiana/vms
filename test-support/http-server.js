const http = require("http");
const { createVmsServer } = require("../backend/server");

function listen(server) {
  return new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () => resolve(server));
  });
}

function close(server) {
  return new Promise((resolve, reject) => server.close((error) => (error ? reject(error) : resolve())));
}

function request(server, { method = "GET", pathname = "/", body } = {}) {
  return new Promise((resolve, reject) => {
    const address = server.address();
    const payload = body === undefined ? null : JSON.stringify(body);
    const req = http.request({
      host: "127.0.0.1",
      port: address.port,
      method,
      path: pathname,
      headers: payload ? {
        "Content-Type": "application/json",
        "Content-Length": Buffer.byteLength(payload)
      } : undefined
    }, (response) => {
      const chunks = [];
      response.on("data", (chunk) => chunks.push(chunk));
      response.on("end", () => {
        const text = Buffer.concat(chunks).toString("utf8");
        const isJson = String(response.headers["content-type"] || "").includes("application/json");
        resolve({
          status: response.statusCode,
          headers: response.headers,
          text,
          body: isJson && text ? JSON.parse(text) : null
        });
      });
    });
    req.on("error", reject);
    if (payload) req.write(payload);
    req.end();
  });
}

async function withServer(options, test) {
  const server = await listen(createVmsServer(options));
  try {
    return await test(server);
  } finally {
    await close(server);
  }
}

module.exports = { close, listen, request, withServer };
