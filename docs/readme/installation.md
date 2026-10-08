<!-- workline
sources: [library.json, library.properties]
-->

# Installing from a checkout

### Specific Components Only

```ini
lib_deps = 
    symlink://path/to/DomoticsCore/DomoticsCore-Core
    symlink://path/to/DomoticsCore/DomoticsCore-LED
    symlink://path/to/DomoticsCore/DomoticsCore-Wifi
```

### A Local Checkout

```ini
lib_deps =
    symlink://../DomoticsCore
```

`symlink://` (PlatformIO 6.1.19 or later) compiles the library where it
lives, so an edit in the checkout is seen by the next build. `file://` copies
the whole repository into `.pio/libdeps` and never refreshes the copy. When a
project switches between a registry version and a checkout, delete its `.pio`
once: PlatformIO keeps the package it already holds.
