Import("env")
# Ensures the gcov runtime is linked on macOS/Linux when --coverage is set.
env.Append(LINKFLAGS=["--coverage"])
