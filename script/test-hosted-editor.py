#!/usr/bin/env python3
"""macOS regression smoke: open/close the installed ATTRACTOR AU editor five times.
Run after building TONE3000_Standalone; uses its exact compiler and linker settings.
The installed third-party plugin is never copied or distributed.
"""
import pathlib
import shlex
import subprocess
source = pathlib.Path(__file__).resolve().parent.parent
build = source / "build"
commands = subprocess.check_output(["ninja", "-C", str(build), "-t", "commands", "TONE3000_Standalone"], text=True).splitlines()
compile_cmd = shlex.split(next(x for x in commands if " -c " in x and "HostedEffectWindow.cpp" in x))
compile_cmd = [x for x in compile_cmd if not x.startswith("-DJucePlugin_Build_Standalone=")]
compile_cmd.insert(1, "-DJucePlugin_Build_Standalone=0")
compile_cmd.insert(1, "-DJUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1")
compile_cmd[compile_cmd.index("-c") + 1] = str(source / "test/hosted-editor-smoke.cpp")
compile_cmd[compile_cmd.index("-o") + 1] = str(build / "hosted-editor-smoke.o")
compile_cmd[compile_cmd.index("-MF") + 1] = str(build / "hosted-editor-smoke.d")
subprocess.run(compile_cmd, cwd=build, check=True)
link = next(x for x in reversed(commands) if " -o " in x and "/MacOS/zynamfx" in x)
link_cmd = shlex.split(link.split(" && ")[1])
link_cmd = [x for x in link_cmd if not ("TONE3000_Standalone.dir" in x and x.endswith(".o"))]
link_cmd.insert(1, str(build / "hosted-editor-smoke.o"))
link_cmd[link_cmd.index("-o") + 1] = str(build / "hosted-editor-smoke")
subprocess.run(link_cmd, cwd=build, check=True)
subprocess.run([str(build / "hosted-editor-smoke")], check=True, timeout=30)
