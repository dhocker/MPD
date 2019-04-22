#! /bin/bash

# Configure
meson . output/release --buildtype=debugoptimized -Db_ndebug=true -Dupnp=disabled

# Disable upnp (how to alter configuration)
# upnp depends on expat and meson cannot find it
meson configure output/release -Dupnp=disabled
# Try building documentation
meson configure output/release -Ddocumentation=true

# Verify unpnp is disabled (list current configuration settings)
meson configure output/release
