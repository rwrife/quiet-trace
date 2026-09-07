import { gzipSync } from "node:zlib";
import { readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";

const DIST_ASSETS_DIR = new URL("../dist/assets/", import.meta.url);
const MAX_TOTAL_RAW_BYTES = 256 * 1024;
const MAX_TOTAL_GZIP_BYTES = 96 * 1024;

const files = readdirSync(DIST_ASSETS_DIR)
  .filter((file) => file.endsWith(".js") || file.endsWith(".css"))
  .sort();

if (files.length === 0) {
  console.error(
    "No JS/CSS assets found in dist/assets. Run `npm run build` first.",
  );
  process.exit(1);
}

let totalRaw = 0;
let totalGzip = 0;

for (const file of files) {
  const absolute = join(DIST_ASSETS_DIR.pathname, file);
  const rawBytes = statSync(absolute).size;
  const gzipBytes = gzipSync(readFileSync(absolute), { level: 9 }).byteLength;
  totalRaw += rawBytes;
  totalGzip += gzipBytes;
  console.log(`${file}: raw=${rawBytes} bytes gzip=${gzipBytes} bytes`);
}

console.log(`TOTAL raw=${totalRaw} bytes gzip=${totalGzip} bytes`);

if (totalRaw > MAX_TOTAL_RAW_BYTES) {
  console.error(
    `Bundle raw size ${totalRaw} exceeds limit ${MAX_TOTAL_RAW_BYTES}. Optimize before firmware embedding.`,
  );
  process.exit(1);
}

if (totalGzip > MAX_TOTAL_GZIP_BYTES) {
  console.error(
    `Bundle gzip size ${totalGzip} exceeds limit ${MAX_TOTAL_GZIP_BYTES}. Optimize before firmware embedding.`,
  );
  process.exit(1);
}

console.log("Bundle size check passed.");
