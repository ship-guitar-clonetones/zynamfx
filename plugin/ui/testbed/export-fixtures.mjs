#!/usr/bin/env node
// Exports the React screenshot suite's fixtures and scenario state into
// plugin/ui/testbed/fixtures/ so the native testbed renders the exact same
// data the reference PNGs were captured from.
//
//   node plugin/ui/testbed/export-fixtures.mjs
//
// The suite itself lives in ui/local/screenshots (local-only tooling). A
// scenario's Playwright `drive` step cannot be exported; Scenarios.cpp mirrors
// those by hand and `hasDrive` flags which ids need one.

import { copyFileSync, mkdirSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const suite = join(here, '../../../ui/local/screenshots');
const out = join(here, 'fixtures');

const fixtures = await import(join(suite, 'fixtures.mjs'));
const { SCENARIOS } = await import(join(suite, 'scenarios.mjs'));

const scenarios = SCENARIOS.map(({ drive, ...s }) => ({
  ...s,
  hints: s.hints !== false,
  banner: !!s.banner,
  auth: !!s.auth,
  hasDrive: typeof drive === 'function',
}));

mkdirSync(join(out, 'img'), { recursive: true });
for (const file of readdirSync(join(suite, 'assets')))
  copyFileSync(join(suite, 'assets', file), join(out, 'img', file));

writeFileSync(
  join(out, 'scenarios.json'),
  JSON.stringify(
    {
      version: '1.4.2',
      imgHost: fixtures.IMG_HOST,
      user: fixtures.FIXTURE_USER,
      apiTones: fixtures.API_TONES,
      updatePayload: fixtures.UPDATE_PAYLOAD,
      gatedPage: fixtures.paginated(fixtures.API_TONES, 1, 3),
      scenarios,
    },
    null,
    1
  )
);
console.log(`exported ${scenarios.length} scenarios to ${out}`);
