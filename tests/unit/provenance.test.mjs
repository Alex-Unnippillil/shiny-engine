import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
test('the user-provided research plan is preserved byte-for-byte', async () => {
  const bytes = await readFile('docs/RESEARCH_AND_IMPLEMENTATION_PLAN.md');
  assert.equal(createHash('sha256').update(bytes).digest('hex'),
    '55295bb0f9b8f62be6f3d1f9295783a38ba9563eff61a9acd4b5cbf734be320a');
});
