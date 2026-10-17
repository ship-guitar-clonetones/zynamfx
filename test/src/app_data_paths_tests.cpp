// App-data root resolution and repair (github issue #76; user report
// "the config doesn't stick").
//
// The reported failure: <user>/Library/Application Support/zynamfx was a
// SYMLINK to a volume that was not mounted. juce::File::exists() stats through
// the link, so the dangling link looked like "nothing here"; the old heal code
// skipped its move-aside branch, called createDirectory() and logged
// "File exists" on every launch. Nothing was ever written: no settings, no
// ui-preferences, no LocalModels stash — while the startup line still claimed
// the folder was writable.
//
// These tests pin the resolver's contract:
//   * a dangling symlink is moved aside (never deleted) and replaced by a real
//     writable directory,
//   * a file squatting on the path is moved aside the same way,
//   * a healthy writable folder is left untouched, and
//   * "writable" is a real create/write/delete probe, not an access() guess.

#include "AppDataPaths.h"

#include <gtest/gtest.h>

#if !JUCE_WINDOWS
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

#if !JUCE_WINDOWS

juce::File makeSandbox() {
  const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("zynamfx-appdata-test-" + juce::Uuid().toString());
  dir.createDirectory();
  return dir;
}

// Creates `linkPath` as a symlink to `targetPath` (which may not exist): the
// dangling-link case that started this.
bool makeSymlink(const juce::File& linkPath, const juce::File& targetPath) {
  return ::symlink(targetPath.getFullPathName().toRawUTF8(),
                   linkPath.getFullPathName().toRawUTF8()) == 0;
}

int countProbeFiles(const juce::File& dir) {
  return dir.findChildFiles(juce::File::findFiles, false, ".write-probe-*").size();
}

TEST(AppDataPathsTest, DanglingSymlinkRootIsMovedAsideAndRecreated) {
  const auto sandbox = makeSandbox();
  ASSERT_TRUE(sandbox.isDirectory());

  const auto root = sandbox.getChildFile(T3K_APP_DATA_DIR_NAME);
  const auto missingTarget = sandbox.getChildFile("unmounted-volume").getChildFile("TONE3000");
  ASSERT_TRUE(makeSymlink(root, missingTarget));
  ASSERT_TRUE(root.isSymbolicLink());
  ASSERT_FALSE(root.exists());  // the whole bug: exists() stats through the link

  EXPECT_TRUE(t3k::appdata::ensureWritableDir(root));

  EXPECT_TRUE(root.isDirectory());
  EXPECT_FALSE(root.isSymbolicLink());
  EXPECT_TRUE(t3k::appdata::probeWritable(root).writable);

  // Nothing deleted: the broken link is preserved as a sibling.
  const auto aside = sandbox.getChildFile(juce::String(T3K_APP_DATA_DIR_NAME) + ".unwritable");
  EXPECT_TRUE(aside.isSymbolicLink());
  EXPECT_EQ(aside.getLinkedTarget().getFullPathName(), missingTarget.getFullPathName());

  sandbox.deleteRecursively();
}

TEST(AppDataPathsTest, SquattingFileIsMovedAsideAndRecreated) {
  const auto sandbox = makeSandbox();
  ASSERT_TRUE(sandbox.isDirectory());

  const auto root = sandbox.getChildFile(T3K_APP_DATA_DIR_NAME);
  ASSERT_TRUE(root.replaceWithText("not a directory"));

  EXPECT_TRUE(t3k::appdata::ensureWritableDir(root));

  EXPECT_TRUE(root.isDirectory());
  const auto aside = sandbox.getChildFile(juce::String(T3K_APP_DATA_DIR_NAME) + ".unwritable");
  EXPECT_TRUE(aside.existsAsFile());
  EXPECT_EQ(aside.loadFileAsString(), juce::String("not a directory"));

  sandbox.deleteRecursively();
}

TEST(AppDataPathsTest, HealthyRootIsLeftAloneAndProbeLeavesNoFiles) {
  const auto sandbox = makeSandbox();
  ASSERT_TRUE(sandbox.isDirectory());

  EXPECT_TRUE(t3k::appdata::ensureWritableDir(sandbox));

  EXPECT_TRUE(sandbox.isDirectory());
  EXPECT_EQ(countProbeFiles(sandbox), 0);
  EXPECT_FALSE(sandbox.getChildFile("unwritable").exists());
  EXPECT_FALSE(sandbox.getSiblingFile(sandbox.getFileName() + ".unwritable").exists());

  sandbox.deleteRecursively();
}

TEST(AppDataPathsTest, ProbeRejectsReadOnlyDirectory) {
  if (::geteuid() == 0)
    GTEST_SKIP() << "root ignores the read-only bits";

  const auto sandbox = makeSandbox();
  ASSERT_TRUE(sandbox.isDirectory());

  ASSERT_EQ(::chmod(sandbox.getFullPathName().toRawUTF8(), 0555), 0);
  const auto denied = t3k::appdata::probeWritable(sandbox);
  EXPECT_FALSE(denied.writable);
  EXPECT_FALSE(denied.reason.isEmpty());

  ASSERT_EQ(::chmod(sandbox.getFullPathName().toRawUTF8(), 0755), 0);
  EXPECT_TRUE(t3k::appdata::probeWritable(sandbox).writable);

  sandbox.deleteRecursively();
}

TEST(AppDataPathsTest, ProbeRejectsMissingDirectory) {
  const auto sandbox = makeSandbox();
  const auto missing = sandbox.getChildFile("does-not-exist");

  const auto result = t3k::appdata::probeWritable(missing);
  EXPECT_FALSE(result.writable);
  EXPECT_FALSE(result.reason.isEmpty());

  sandbox.deleteRecursively();
}

#endif  // !JUCE_WINDOWS

TEST(AppDataPathsTest, FallbackRootIsSeparateFromPrimaryRoot) {
  const auto primary = t3k::appdata::primaryRoot();
  const auto fallback = t3k::appdata::fallbackRoot();

  EXPECT_NE(primary.getFullPathName(), fallback.getFullPathName());
  EXPECT_FALSE(fallback.isAChildOf(primary));
  EXPECT_TRUE(fallback.getFileName().endsWith("-local"));
}

}  // namespace
