// Optional artwork maintenance only: the game build uses committed PNG/ICO files.
// Run with Node.js and ImageMagick 7 on PATH: node tools/generate-icons.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const assets = path.join(root, 'assets', 'icons');
const scratch = path.join(root, 'out', 'icon-generation');
fs.mkdirSync(scratch, { recursive: true });
const source = fs.readFileSync(path.join(assets, 'arrow-puzzle.svg'), 'utf8');
const baseWidth = Number(source.match(/stroke-width="([\d.]+)"/)[1]);
const sizes = [16, 24, 32, 48, 64, 128, 256];

function roundedTriangle(points, radius) {
    const fmt = value => Number(value.toFixed(4));
    const corners = points.map((v, i) => {
        const prev = points[(i + 2) % 3], next = points[(i + 1) % 3];
        const lp = Math.hypot(prev[0] - v[0], prev[1] - v[1]);
        const ln = Math.hypot(next[0] - v[0], next[1] - v[1]);
        const u = [(prev[0] - v[0]) / lp, (prev[1] - v[1]) / lp];
        const w = [(next[0] - v[0]) / ln, (next[1] - v[1]) / ln];
        const theta = Math.acos(Math.max(-1, Math.min(1, u[0] * w[0] + u[1] * w[1])));
        const setback = radius / Math.tan(theta / 2);
        if (setback >= Math.min(lp, ln) / 2) throw Error('Icon corner radius is too large');
        return {
            before: [v[0] + u[0] * setback, v[1] + u[1] * setback],
            after: [v[0] + w[0] * setback, v[1] + w[1] * setback],
        };
    });
    let result = 'M ' + corners[0].before.map(fmt).join(' ');
    for (let i = 0; i < 3; ++i) {
        if (i) result += ' L ' + corners[i].before.map(fmt).join(' ');
        result += ` A ${radius} ${radius} 0 0 1 ` + corners[i].after.map(fmt).join(' ');
    }
    return result + ' Z';
}

for (const size of [...sizes, 1024]) {
    const width = baseWidth * (size === 16 ? 64 / 52 : size === 24 ? 58 / 52 : 1);
    let svg = source.replace(/<svg\b[^>]*>/, tag => tag
        .replace(/width="[^"]*"/, `width="${size}"`)
        .replace(/height="[^"]*"/, `height="${size}"`));
    svg = svg.replace(/stroke-width="[\d.]+"/g, `stroke-width="${width}"`);
    svg = svg.replace(/<circle\b[^>]*>/g, tag => tag.replace(/\br="[^"]*"/, `r="${width / 2}"`));
    if (size <= 32) svg = svg.replace('<g fill="#374b65">', '<g fill="#4e617b">');
    let heads = 0;
    svg = svg.replace(/<path\b[^>]*data-corners="([^"]+)"[^>]*\/>/g, (tag, vertices) => {
        ++heads;
        const points = vertices.split(' ').map(p => p.split(',').map(Number));
        return tag.replace(/\bd="[^"]*"/, `d="${roundedTriangle(points, width / 4)}"`);
    });
    if (heads !== 2) throw Error('The SVG must retain data-corners on both arrowheads');
    const input = path.join(scratch, `icon-${size}.svg`);
    const output = path.join(assets, size === 1024 ? 'arrow-puzzle.png' : `arrow-puzzle-${size}.png`);
    fs.writeFileSync(input, svg);
    execFileSync('magick', ['-background', 'none', '-density', '384', input,
        '-resize', `${size}x${size}`, '-depth', '8', '-strip', `PNG32:${output}`], { windowsHide: true });
}

// ICO stores the exact optically adjusted PNGs used by the window, not resizes
// of a single large raster. Windows accepts PNG-compressed icon entries.
const header = Buffer.alloc(6 + 16 * sizes.length);
header.writeUInt16LE(1, 2);
header.writeUInt16LE(sizes.length, 4);
const frames = sizes.map(size => fs.readFileSync(path.join(assets, `arrow-puzzle-${size}.png`)));
let offset = header.length;
frames.forEach((frame, index) => {
    const entry = 6 + 16 * index;
    header[entry] = header[entry + 1] = sizes[index] === 256 ? 0 : sizes[index];
    header.writeUInt16LE(1, entry + 4);
    header.writeUInt16LE(32, entry + 6);
    header.writeUInt32LE(frame.length, entry + 8);
    header.writeUInt32LE(offset, entry + 12);
    offset += frame.length;
});
fs.writeFileSync(path.join(assets, 'arrow-puzzle.ico'), Buffer.concat([header, ...frames]));
console.log('Updated embedded PNGs and ICO:', sizes.join(', '));
