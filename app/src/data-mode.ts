export type DataMode = "device" | "fixture";

export function resolveDataMode(url: URL): DataMode {
  return url.searchParams.get("fixture") === "1" ? "fixture" : "device";
}
