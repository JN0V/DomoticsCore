# PlatformIO extra script to force DomoticsCore to be re-copied from local path
# Only remove libdeps to avoid SCons database corruption

import os
import shutil
import time
from SCons.Script import Import, DefaultEnvironment

# Import injects 'env' into globals; do not assign its return value (it's None)
Import("env")
env = DefaultEnvironment()
project_dir = env["PROJECT_DIR"]
pioenv = env["PIOENV"]

LIBDEPS_DIR = os.path.join(project_dir, ".pio", "libdeps", pioenv)
DOMOTICSCORE_DIR = os.path.join(LIBDEPS_DIR, "DomoticsCore")

# Run cleanup immediately when script loads
print("[extra_script] Library cleanup starting...")

# Only remove DomoticsCore library to force re-copy from local path
try:
    if os.path.isdir(DOMOTICSCORE_DIR):
        print(f"[extra_script] Removing DomoticsCore library: {DOMOTICSCORE_DIR}")
        shutil.rmtree(DOMOTICSCORE_DIR)
    else:
        print(f"[extra_script] DomoticsCore library doesn't exist: {DOMOTICSCORE_DIR}")
except Exception as e:
    print(f"[extra_script] Warning: failed to remove DomoticsCore: {e}")

# Add build UID to force recompilation
build_uid = int(time.time())
env.Append(BUILD_FLAGS=[f"-DBUILD_UID={build_uid}"])
print(f"[extra_script] Added BUILD_UID={build_uid}")
print("[extra_script] DomoticsCore will be re-copied from local source")
