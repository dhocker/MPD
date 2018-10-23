#! /bin/bash

# Configure
meson . output/release --buildtype=debugoptimized -Db_ndebug=true

# Disable upnp (how to alter configuration)
# upnp depends on expat and meson cannot find it
meson configure output/release -Dupnp=disabled

# Verify unpnp is disabled (list current configuration settings)
meson configure output/release
