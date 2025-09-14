# PlatformIO extra script to force DomoticsCore library refresh
# This ensures local changes are always picked up

import os
import time
from SCons.Script import Import, DefaultEnvironment

# Import injects 'env' into globals; do not assign its return value (it's None)
Import("env")
env = DefaultEnvironment()

def before_build(source, target, env):
    print("[extra_script] Forcing library refresh for local development...")
    # Add build UID to force recompilation of library
    build_uid = int(time.time())
    env.Append(BUILD_FLAGS=[f"-DBUILD_UID={build_uid}"])
    print(f"[extra_script] Added BUILD_UID={build_uid}")

# Hook into the build process before compilation starts
env.AddPreAction("$BUILD_DIR/src/main.cpp.o", before_build)
