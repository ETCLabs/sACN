rm -rf build
mkdir build
cd build
cmake -G "Unix Makefiles" -DSACN_BUILD_TESTS=ON -DSACN_BUILD_EXAMPLES=ON ..
make -j `nproc`
