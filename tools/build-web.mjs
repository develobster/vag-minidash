import { mkdir, copyFile, readdir, readFile } from "node:fs/promises";
const source = new URL("../minidash_live/web/", import.meta.url),
  target = new URL("../dist/web/", import.meta.url);
await mkdir(target, { recursive: true });
for (const name of await readdir(source))
  await copyFile(new URL(name, source), new URL(name, target));
const manifest = JSON.parse(
  await readFile(new URL("manifest.webmanifest", target), "utf8"),
);
for (const icon of manifest.icons)
  await readFile(new URL(icon.src.slice(1), target));
console.log(
  "Production static assets validated and copied to dist/web (no runtime npm dependencies).",
);
