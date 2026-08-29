#!/usr/bin/env python
import os

env = SConscript("godot-cpp/SConstruct")

# Opus headers and our sources
env.Append(CPPPATH=["src/", "opus/"])

# Link against the prebuilt opus library for the target platform
if env["platform"] == "linux":
    env.Append(LIBPATH=["libs/linux/"])
    env.Append(LIBS=["opus"])
    # Find the bundled libopus.so.0 next to the extension at runtime
    env.Append(LINKFLAGS=["-Wl,-rpath,'$$ORIGIN'"])
elif env["platform"] == "macos":
    env.Append(LIBPATH=["libs/osx/"])
    env.Append(LIBS=["opus"])
elif env["platform"] == "windows":
    env.Append(LIBPATH=["libs/win_x64/"])
    env.Append(LIBS=["opus"])
elif env["platform"] == "android":
    env.Append(LIBPATH=["libs/android/" + env["arch"]])
    env.Append(LIBS=["opus"])

sources = Glob("src/*.cpp")

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "bin/libgodotopus.{}.{}.framework/libgodotopus.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "bin/libgodotopus{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

Default(library)
