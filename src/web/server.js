/* server.js
 * Simple HTTP server for serving a Pebble game from your workspace.
 * TODO Onboard a C HTTP server and have pbltool replace this.
 *
 * GET /rom returns the ROM file verbatim, optionally running `make` first.
 * Any other GET serves out of --htdocs.
 * No other methods.
 */
 
const fs = require("fs/promises");
const http = require("http");
const child_process = require("child_process");

/* Config.
 *********************************************************/
 
let rom = ""; // Path or empty.
let make = false; // Should we run `make` before serving (rom)?
let htdocs = "src/web";
let port = 8080;
let host = "localhost";

for (let argi=2; argi<process.argv.length; argi++) {
  const arg = process.argv[argi];
  if (arg.startsWith("--rom=")) {
    if (rom) throw new Error(`Multiple --rom. We can only do one at a time.`);
    rom = arg.substring(6);
  } else if (arg === "--make") {
    make = true;
  } else if (arg.startsWith("--htdocs=")) {
    htdocs = arg.substring(9);
  } else if (arg.startsWith("--port=")) {
    port = +arg.substring(7);
  } else if (arg === "--help") {
    console.log(`Usage: ${process.argv[1]} --rom=PATH [--make] [--htdocs=src/web] [--port=PORT]`);
    process.exit(0);
  }
}
if (!rom) throw new Error(`Please provide ROM file as '--rom=PATH'`);

/* Generic file service.
 ********************************************************/
 
function guessContentType(path, serial) {
  const dotSplit = path.split('.');
  if (dotSplit.length >= 2) {
    const sfx = dotSplit[dotSplit.length - 1].toLowerCase();
    switch (sfx) {
      case "html": return "text/html";
      case "ico": return "image/x-icon";
      case "js": return "application/javascript";
      case "css": return "text/css";
      case "png": return "image/png";
      case "pbl": return "application/x-pebble-rom";
    }
  }
  return "application/octet-stream";
}

// Caller validates (path); it is in the host filesystem.
function serveFile(path, req, rsp) {
  fs.readFile(path).then((serial) => {
    rsp.setHeader("Content-Type", guessContentType(path, serial));
    rsp.statusCode = 200;
    rsp.end(serial);
  }).catch((error) => {
    console.error(error);
    rsp.statusCode = 404;
    rsp.end(`File not found: ${req.url}`);
  });
}

/* Invoke make, then serve the ROM file.
 * Or serve make's output, if it fails.
 **************************************************/
 
function serveRom(path, req, rsp) {
  const child = child_process.spawn("make", [path]);
  let log = "";
  child.stdout.on("data", v => { log += v; });
  child.stderr.on("data", v => { log += v; });
  child.on("close", status => {
    if (status === 0) {
      serveFile(path, req, rsp);
    } else {
      rsp.statusCode = 599;
      rsp.statusText = `make failed`;
      rsp.setHeader("Content-Type", "text/plain");
      rsp.end(log);
    }
  });
}
 
/* Setup.
 ***********************************************/
 
const server = http.createServer((req, rsp) => {
  if (req.method !== "GET") {
    rsp.statusCode = 405;
    rsp.end("GET only, please\n");
    return;
  }
  let path = req.url.split('?')[0];
  if (!path || (path === "/")) path = "/index.html";
  if (!path.startsWith("/") || path.includes("..")) {
    rsp.statusCode = 400;
    rsp.end("Invalid path");
    return;
  }
  if (make && rom && (path === "/rom")) {
    serveRom(rom, req, rsp);
  } else {
    serveFile(htdocs + path, req, rsp);
  }
});

server.listen(port, host, (error) => {
  if (error) {
    console.error(error);
  } else {
    console.log(`Listening on ${host}:${port}`);
  }
});
