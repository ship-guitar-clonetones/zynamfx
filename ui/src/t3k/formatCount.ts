/**
 * Compact social-style counts (same as TONE3000's formatCount).
 *
 * Exact under 10,000 (max 4 digits, with grouping separators). Larger values
 * use K / M / B with at most 4 significant digits:
 *
 *   123       -> "123"
 *   1,234     -> "1,234"
 *   12,300    -> "12.3K"
 *   123,400   -> "123.4K"
 *   1,234,000 -> "1.234M"
 */

const COMPACT_THRESHOLD = 10_000;

const compactFormatter = new Intl.NumberFormat('en', {
  notation: 'compact',
  compactDisplay: 'short',
  maximumSignificantDigits: 4,
});

function toCount(value: number | null | undefined): number {
  if (value == null || !Number.isFinite(value) || value < 0) return 0;
  return Math.round(value);
}

export function formatCount(value: number | null | undefined): string {
  const n = toCount(value);
  if (n < COMPACT_THRESHOLD) return n.toLocaleString('en-US');
  return compactFormatter.format(n);
}
