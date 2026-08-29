#!/usr/bin/env python
import os

env = SConscript("godot-cpp/SConstruct")

env.Append(CPPPATH=["src/", "thirdparty/opus/include/"])

# Static libopus built from the submodule, see .github/workflows/build.yml
# for the cmake invocation (or run it locally with the same flags)
opus_build_dir = "thirdparty/opus/build/{}.{}".format(env["platform"], env["arch"])
env.Append(LIBPATH=[opus_build_dir])
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
