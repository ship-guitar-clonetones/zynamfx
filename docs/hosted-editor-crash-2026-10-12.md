# ATTRACTOR native editor attachment crash

The supplied macOS incident 37FB6AFE-B4F8-4C5F-ABA8-2D8D39D4F363 crashed in ATTRACTOR 1.0.0, IGraphics::SetScreenScale, during viewDidMoveToWindow. The host stack shows setUsingNativeTitleBar recreating the JUCE desktop peer after setContentOwned attached the AU editor.

The host now finalises its native title bar before creating or attaching a third-party editor. Closing the window still destroys the editor before releasing its shared plugin instance. DSP worker thread labels now use zynamfx.

Validation: standalone Release rebuilt and the complete Desktop bundle passed strict ad hoc signature verification. The new script/test-hosted-editor.py uses the standalone compiler/linker settings and the installed ATTRACTOR AU, opening and closing the actual host window five times. All five cycles passed. This is a targeted regression smoke, not a guarantee that every third-party editor is safe. No third-party plugin binary is included in the source archive.

Run after the standalone build with cmake/ninja on PATH:

```sh
python3 script/test-hosted-editor.py
```

The harness compiles with JucePlugin_Build_Standalone=0 to generate its own main; the application code uses the existing standalone build unchanged. Avoid adding the source root to compiler -I on case-insensitive macOS filesystems: VERSION would shadow the standard C++ <version> header. Vendored source archives already include dependencies and must use the archive build path, rather than attempting Git submodule initialisation over populated non-Git dependency directories.
