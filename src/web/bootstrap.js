import { Runtime } from "./Runtime.js";

function displayGeneralError(error) {
  if (!error) {
    error = "Unspecified error.";
  } else if (typeof(error) === "string") {
  } else if (error.stack) {
    error = (error.message || "") + "\n" + error.stack;
  } else if (error.message) {
    error = error.message;
  } else {
    try {
      error = JSON.stringify(error, null, 2);
    } catch (ee) {
      error = "Unrepresentable error.";
    }
  }
  const body = document.body;
  body.innerHTML = "";
  body.classList.add("fatal");
  body.innerText = error;
}

function displayMakeError(log) {
  const body = document.body;
  body.innerHTML = "";
  body.classList.add("fatal");
  body.innerText = log;
}

function loadGame(serial) {
  const runtime = new Runtime(document.body, serial);
  return runtime.start();
}

window.addEventListener("load", () => {
  // Any synchronous startup errors, eg failures to decode the ROM, will throw in this block.
  // Errors after startup do not participate. (TODO should they?)
  fetch("/rom").then(rsp => {
    if (rsp.status === 599) {
      return rsp.text().then(displayMakeError);
    } else {
      return rsp.arrayBuffer().then(loadGame);
    }
  }).catch(displayGeneralError);
}, { once: true });
