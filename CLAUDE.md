**SACN_SANDBOX_MODE=ON is mandatory.** Without it, CMake will try to fetch dependencies (GoogleTest, etc.) from the internal ETC GitLab over SSH, which will fail in this environment. Always pass `-DSACN_SANDBOX_MODE=ON` to every CMake invocation — configure and build will silently break without it.

CMake was generated in a different folder, so if it complains about paths not matching you'll need to use `sudo ln -s` to create a symlink for the path it wants to the actual pwd.

## Building

The Bash tool's cwd is a Windows UNC-style path (`/wsl.localhost/...`) that Linux tools can't resolve. Always run cmake and make from a real Linux path:

```bash
# One-time setup: create symlink if /home/chreese/sacn doesn't exist
sudo mkdir -p /home/chreese
sudo ln -s /wsl.localhost/Ubuntu/home/chreese/sacn /home/chreese/sacn

# Configure (run from /tmp to avoid cwd issues)
bash -c "cd /tmp && cmake /home/chreese/sacn -B /home/chreese/sacn/build \
  -DSACN_SANDBOX_MODE=ON \
  -DCMAKE_C_COMPILER=/usr/bin/gcc-13 \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-13"

# Build
bash -c "cd /tmp && cmake --build /home/chreese/sacn/build -j$(nproc)"
```

The C++ compiler must be specified explicitly — `/usr/bin/c++` is not resolvable by cmake in this environment; use `/usr/bin/g++-13`.
