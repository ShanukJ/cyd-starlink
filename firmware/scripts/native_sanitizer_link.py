# PlatformIO passes -fsanitize only to the compiler; the linker needs it too.
Import("env")  # noqa: F821

env.Append(LINKFLAGS=["-fsanitize=address,undefined"])  # noqa: F821
