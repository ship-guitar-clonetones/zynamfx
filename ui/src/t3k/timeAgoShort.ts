/** Compact relative time (e.g. "13h", "3d", "2w") for tone creator lines. */
export const timeAgoShort = (dateString: string) => {
  const date = new Date(dateString);
  const now = new Date();
  const seconds = Math.floor((now.getTime() - date.getTime()) / 1000);
  const MINUTE = 60;
  const HOUR = MINUTE * 60;
  const DAY = HOUR * 24;
  const WEEK = DAY * 7;
  const MONTH = WEEK * 4;
  const YEAR = MONTH * 12;
  if (seconds < 10) return 'now';
  if (seconds < MINUTE) return `${seconds}s`;
  if (seconds < HOUR) return `${Math.floor(seconds / MINUTE)}m`;
  if (seconds < DAY) return `${Math.floor(seconds / HOUR)}h`;
  if (seconds < WEEK) return `${Math.floor(seconds / DAY)}d`;
  if (seconds < MONTH) return `${Math.floor(seconds / WEEK)}w`;
  if (seconds < YEAR) return `${Math.floor(seconds / MONTH)}mo`;
  return `${Math.floor(seconds / YEAR)}y`;
};
