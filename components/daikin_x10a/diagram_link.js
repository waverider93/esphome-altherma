// Added to the ESPHome web page (web_server js_include) by daikin_x10a when diagram: true:
// a button at the top right that opens the drawing of the installation on /altherma/.
const label = {de: "Anlagenbild", fr: "Schéma", it: "Schema", es: "Esquema"}[navigator.language.slice(0, 2)] || "Diagram";
const link = document.createElement("a");
link.href = "/altherma/";
link.textContent = label;
link.style.cssText =
  "position:fixed;top:12px;right:16px;z-index:10;padding:6px 14px;border-radius:999px;background:#0466c8;" +
  "color:#fff;font:600 14px system-ui,sans-serif;text-decoration:none;box-shadow:0 2px 8px rgba(0,0,0,.2)";
document.body.appendChild(link);
