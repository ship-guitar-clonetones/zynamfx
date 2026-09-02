// Local file loading (loadLocalTone for drops, loadLocalTonePath for the
// tile menus' file pickers): load-time validation (NAM files must be A2),
// and that accepted files ride the normal load pipeline as `local` blocks:
// background load from the content-addressed stash (no network), IR mix
// defaults by kernel length, folder loads as one multi-model tone whose
// full model list survives switches.
//
// Note: both entry points write their stash into the real app-data
// LocalModels folder. The names are content hashes, so repeated runs reuse
// the same few small files (and the week-based GC clears them eventually).

#include "chain_test_helpers.h"

#include <gtest/gtest.h>

namespace {

juce::String base64Of(const juce::File& file) {
  juce::MemoryBlock bytes;
  EXPECT_TRUE(file.loadFileAsData(bytes));
  return juce::Base64::toBase64(bytes.getData(), bytes.getSize());
}

// One { name, data } entry of the files array the UI ships.
juce::var fileEntry(const juce::String& name, const juce::String& base64) {
  juce::DynamicObject::Ptr entry = new juce::DynamicObject();
  entry->setProperty("name", name);
  entry->setProperty("data", base64);
  return juce::var(entry.get());
}

juce::var testFileEntry(const char* name) { return fileEntry(name, base64Of(testFile(name))); }

juce::var filesOf(const juce::Array<juce::var>& entries) { return juce::var(entries); }

// First tone block of the (mono) chain, or void when none.
juce::var firstToneBlock(TONE3000Processor& proc) {
  const juce::var state = proc.getChainState(-1);
  if (const auto* lane = state["chain"].getArray())
    for (const auto& item : *lane)
      if (item["kind"].toString() == "tone")
        return item;
  return {};
}

}  // namespace

TEST(LocalLoadTest, A2NamFileLoadsAsLocalBlock) {
  TONE3000Processor proc;
  const juce::var res =
      proc.loadLocalTone("a2-amp-test", filesOf({testFileEntry("a2-amp-test.nam")}));
  EXPECT_TRUE(res["error"].isVoid()) << res["error"].toString().toStdString();
  ASSERT_TRUE(res["blockId"].toString().isNotEmpty());
  ASSERT_TRUE(waitForChainLoaded(proc));

  const juce::var block = firstToneBlock(proc);
  EXPECT_EQ(block["blockId"].toString(), res["blockId"].toString());
  EXPECT_TRUE(static_cast<bool>(block["tone"]["local"]));
  EXPECT_EQ(block["tone"]["format"].toString(), juce::String("nam"));
  EXPECT_EQ(block["tone"]["title"].toString(), juce::String("a2-amp-test"));
  // Model names drop the extension too.
  ASSERT_EQ(block["tone"]["models"].size(), 1);
  EXPECT_EQ(block["tone"]["models"][0]["name"].toString(), juce::String("a2-amp-test"));
}

TEST(LocalLoadTest, FolderLoadsAsOneTonePerFileModelsSurviveSwitch) {
  TONE3000Processor proc;
  const juce::var res = proc.loadLocalTone(
      "My Pack", filesOf({testFileEntry("a2-amp-test.nam"), testFileEntry("a2-amp-cab-test.nam")}));
  EXPECT_TRUE(res["error"].isVoid()) << res["error"].toString().toStdString();
  ASSERT_TRUE(waitForChainLoaded(proc));

  juce::var block = firstToneBlock(proc);
  EXPECT_EQ(block["tone"]["title"].toString(), juce::String("My Pack"));
  ASSERT_EQ(block["tone"]["models"].size(), 2);
  // Local summary models carry their stash URL: it's what the picker's
  // switch call sends back down.
  const juce::var second = block["tone"]["models"][1];
  ASSERT_TRUE(second["model_url"].toString().startsWith("file://"));

  ASSERT_TRUE(proc.switchModel(block["blockId"].toString().toStdString(),
                               static_cast<int>(second["id"]), second));
  ASSERT_TRUE(waitForChainLoaded(proc));

  // The switch keeps the whole local model list (catalog tones would prune
  // to the picked model here).
  block = firstToneBlock(proc);
  EXPECT_EQ(static_cast<int>(block["activeModelId"]), static_cast<int>(second["id"]));
  EXPECT_EQ(block["tone"]["models"].size(), 2);
}

TEST(LocalLoadTest, DropOnExistingToneBlockSwapsInPlace) {
  TONE3000Processor proc;
  const juce::var first =
      proc.loadLocalTone("a2-amp-test", filesOf({testFileEntry("a2-amp-test.nam")}));
  ASSERT_TRUE(first["blockId"].toString().isNotEmpty());
  ASSERT_TRUE(waitForChainLoaded(proc));
  const juce::String blockId = first["blockId"].toString();

  const juce::var swapped = proc.loadLocalTone(
      "cab-ir-test", filesOf({testFileEntry("cab-ir-test.wav")}), blockId.toStdString());
  EXPECT_TRUE(swapped["error"].isVoid()) << swapped["error"].toString().toStdString();
  EXPECT_EQ(swapped["blockId"].toString(), blockId);
  ASSERT_TRUE(waitForChainLoaded(proc));

  const juce::var block = firstToneBlock(proc);
  EXPECT_EQ(block["blockId"].toString(), blockId);
  EXPECT_EQ(block["tone"]["title"].toString(), juce::String("cab-ir-test"));
  EXPECT_EQ(block["tone"]["format"].toString(), juce::String("ir"));

  // Still a single tone block (didn't insert a second).
  const juce::var state = proc.getChainState(-1);
  int tones = 0;
  if (const auto* lane = state["chain"].getArray())
    for (const auto& item : *lane)
      if (item["kind"].toString() == "tone")
        ++tones;
  EXPECT_EQ(tones, 1);
}

TEST(LocalLoadTest, IrMixDefaultsFollowKernelLength) {
  // Short (cab) IR: fully wet by default.
  {
    TONE3000Processor proc;
    proc.loadLocalTone("cab-ir-test", filesOf({testFileEntry("cab-ir-test.wav")}));
    ASSERT_TRUE(waitForChainLoaded(proc));
    const juce::var block = firstToneBlock(proc);
    EXPECT_FALSE(static_cast<bool>(block["irLong"]));
    EXPECT_FLOAT_EQ(static_cast<float>(block["params"]["mix"]), 1.0f);
  }
  // Long (reverb) IR: half wet by default, same as a Select-flow load.
  {
    TONE3000Processor proc;
    proc.loadLocalTone("reverb-ir-mono-test", filesOf({testFileEntry("reverb-ir-mono-test.wav")}));
    ASSERT_TRUE(waitForChainLoaded(proc));
    const juce::var block = firstToneBlock(proc);
    EXPECT_TRUE(static_cast<bool>(block["irLong"]));
    EXPECT_FLOAT_EQ(static_cast<float>(block["params"]["mix"]), 0.5f);
  }
}

TEST(LocalLoadTest, RejectsBadFilesAndSkipsThemInFolders) {
  TONE3000Processor proc;

  // Valid JSON, wrong architecture: rejected at drop time (never reaches
  // the background loader, whose failure UI would suggest retrying).
  const juce::String lstm = R"({"version":"0.5.4","architecture":"LSTM","config":{}})";
  const juce::String lstm64 = juce::Base64::toBase64(lstm.toRawUTF8(), lstm.getNumBytesAsUTF8());
  juce::var res = proc.loadLocalTone("model", filesOf({fileEntry("model.nam", lstm64)}));
  EXPECT_EQ(res["error"].toString(), juce::String("Only A2 NAM files are supported"));

  res = proc.loadLocalTone("bad", filesOf({fileEntry("bad.wav", juce::Base64::toBase64("x", 1))}));
  EXPECT_EQ(res["error"].toString(), juce::String("Not a valid WAV file"));

  res = proc.loadLocalTone("notes",
                           filesOf({fileEntry("notes.txt", juce::Base64::toBase64("hi", 2))}));
  EXPECT_EQ(res["error"].toString(), juce::String("Only .nam and .wav files are supported"));

  // A rejected drop must not leave a block behind.
  EXPECT_TRUE(firstToneBlock(proc).isVoid());

  // A folder with one bad file still loads the good ones.
  res = proc.loadLocalTone(
      "Mixed Pack", filesOf({fileEntry("model.nam", lstm64), testFileEntry("a2-amp-test.nam")}));
  EXPECT_TRUE(res["error"].isVoid()) << res["error"].toString().toStdString();
  ASSERT_TRUE(waitForChainLoaded(proc));
  EXPECT_EQ(firstToneBlock(proc)["tone"]["models"].size(), 1);
}

// loadLocalTonePath: the tile menus' Load File / Load Folder pickers. Same
// pipeline as loadLocalTone but fed from disk paths (native FileChooser
// results); the folder rules (majority extension, natural order, title from
// the folder name) live natively here instead of in the web UI.

TEST(LocalLoadTest, PathLoadsSingleFileAndSwapsInPlace) {
  TONE3000Processor proc;
  const juce::var res = proc.loadLocalTonePath(testFile("a2-amp-test.nam"));
  EXPECT_TRUE(res["error"].isVoid()) << res["error"].toString().toStdString();
  ASSERT_TRUE(res["blockId"].toString().isNotEmpty());
  ASSERT_TRUE(waitForChainLoaded(proc));

  juce::var block = firstToneBlock(proc);
  EXPECT_TRUE(static_cast<bool>(block["tone"]["local"]));
  EXPECT_EQ(block["tone"]["format"].toString(), juce::String("nam"));
  EXPECT_EQ(block["tone"]["title"].toString(), juce::String("a2-amp-test"));

  // Targeting an existing tone block replaces in place, like a drop on a tile.
  const juce::String blockId = res["blockId"].toString();
  const juce::var swapped =
      proc.loadLocalTonePath(testFile("cab-ir-test.wav"), blockId.toStdString());
  EXPECT_EQ(swapped["blockId"].toString(), blockId);
  ASSERT_TRUE(waitForChainLoaded(proc));
  block = firstToneBlock(proc);
  EXPECT_EQ(block["blockId"].toString(), blockId);
  EXPECT_EQ(block["tone"]["format"].toString(), juce::String("ir"));
  EXPECT_EQ(block["tone"]["title"].toString(), juce::String("cab-ir-test"));
}

TEST(LocalLoadTest, PathLoadsFolderMajorityExtensionInNaturalOrder) {
  // Scratch folder: three distinct .nam files whose natural order differs
  // from lexicographic ("amp 10" sorts before "amp 2" there), one of them in
  // a subfolder (recursion), plus a minority .wav and a stray .txt (ignored).
  const juce::File dir =
      juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("t3k-pack-test");
  dir.deleteRecursively();
  ASSERT_TRUE(dir.getChildFile("More").createDirectory());
  ASSERT_TRUE(testFile("a2-amp-test.nam").copyFileTo(dir.getChildFile("amp 2.nam")));
  ASSERT_TRUE(testFile("a2-amp-cab-test.nam").copyFileTo(dir.getChildFile("amp 10.nam")));
  ASSERT_TRUE(
      testFile("a2-am-test-2.nam").copyFileTo(dir.getChildFile("More").getChildFile("amp 1.nam")));
  ASSERT_TRUE(testFile("cab-ir-test.wav").copyFileTo(dir.getChildFile("cab.wav")));
  ASSERT_TRUE(dir.getChildFile("notes.txt").replaceWithText("hi"));

  TONE3000Processor proc;
  const juce::var res = proc.loadLocalTonePath(dir);
  EXPECT_TRUE(res["error"].isVoid()) << res["error"].toString().toStdString();
  ASSERT_TRUE(waitForChainLoaded(proc));

  const juce::var block = firstToneBlock(proc);
  EXPECT_EQ(block["tone"]["title"].toString(), juce::String("t3k-pack-test"));
  EXPECT_EQ(block["tone"]["format"].toString(), juce::String("nam"));
  ASSERT_EQ(block["tone"]["models"].size(), 3)
      << "models: " << juce::JSON::toString(block["tone"]["models"]).toStdString();
  EXPECT_EQ(block["tone"]["models"][0]["name"].toString(), juce::String("amp 1"));
  EXPECT_EQ(block["tone"]["models"][1]["name"].toString(), juce::String("amp 2"));
  EXPECT_EQ(block["tone"]["models"][2]["name"].toString(), juce::String("amp 10"));

  dir.deleteRecursively();
}

TEST(LocalLoadTest, PathRejectsBadInputs) {
  TONE3000Processor proc;

  juce::var res = proc.loadLocalTonePath(juce::File("/nonexistent-t3k/missing.nam"));
  EXPECT_EQ(res["error"].toString(), juce::String("Couldn't read the file"));

  const juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("t3k-pack-reject-test");
  dir.deleteRecursively();
  ASSERT_TRUE(dir.createDirectory().wasOk());
  ASSERT_TRUE(dir.getChildFile("notes.txt").replaceWithText("hi"));

  res = proc.loadLocalTonePath(dir.getChildFile("notes.txt"));
  EXPECT_EQ(res["error"].toString(), juce::String("Only .nam and .wav files are supported"));

  // A folder whose only contents are unsupported types has nothing to load.
  res = proc.loadLocalTonePath(dir);
  EXPECT_EQ(res["error"].toString(), juce::String("No .nam or .wav files in the folder"));

  // Rejected loads must not leave a block behind.
  EXPECT_TRUE(firstToneBlock(proc).isVoid());
  dir.deleteRecursively();
}
