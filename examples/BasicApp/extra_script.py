# PlatformIO extra script to force DomoticsCore to be re-copied from local path
# and to inject a unique BUILD_UID define to invalidate compilation cache.
# This prevents stale binaries when iterating quickly on the local library.

import os
import shutil
import time
from SCons.Script import Import, DefaultEnvironment

# Import injects 'env' into globals; do not assign its return value (it's None)
Import("env")
env = DefaultEnvironment()
project_dir = env["PROJECT_DIR"]
pioenv = env["PIOENV"]

# Location where file:// dependencies are copied
LIBDEPS_DIR = os.path.join(project_dir, ".pio", "libdeps", pioenv)
DOMOTICSCORE_DIR = os.path.join(LIBDEPS_DIR, "DomoticsCore")


def pre_build_cleanup(source, target, env):
    try:
        if os.path.isdir(DOMOTICSCORE_DIR):
            print("[extra_script] Removing cached DomoticsCore in libdeps to force re-import …")
            shutil.rmtree(DOMOTICSCORE_DIR)
    except Exception as e:
        print(f"[extra_script] Warning: failed to remove cached DomoticsCore: {e}")

    # Add a per-build UID define to ensure a recompile of translation units
    build_uid = int(time.time())
    env.Append(BUILD_FLAGS=[f"-DBUILD_UID={build_uid}"])
    print(f"[extra_script] Added BUILD_UID={build_uid}")


# Hook before the program build
env.AddPreAction("buildprog", pre_build_cleanup)
