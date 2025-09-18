Import("env")

# Simple script to ensure library is refreshed on each build
# This helps with header-only library development

def refresh_library(source, target, env):
    print("Refreshing DomoticsCore library...")

env.AddPreAction("buildprog", refresh_library)
