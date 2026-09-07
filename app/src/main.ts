import "./style.css";
import { DashboardApp } from "./dashboard-app";

const appRoot = document.querySelector<HTMLElement>("#app");

if (appRoot === null) {
  throw new Error("Quiet Trace app root (#app) not found");
}

const app = new DashboardApp(appRoot);
app.init().catch((error) => {
  appRoot.innerHTML = `<div class="card"><h1>Quiet Trace dashboard error</h1><p role="alert">${error instanceof Error ? error.message : String(error)}</p></div>`;
});

window.addEventListener("beforeunload", () => {
  app.dispose();
});
