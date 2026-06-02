Make sure to set -DSACN_SANDBOX_MODE=ON when invoking CMake.

CMake was generated in a different folder, so if it complains about paths not matching you'll need to use sudo ln -s to create a symlink for the path it wants to the actual pwd.
