## Building the indexer
For the indexer to work, one has to clone the MemObjStorage library first.
Make sure to clone it into the same directory as the indexer.
https://github.com/sifeiler/MemObjStorage

After that, run `build_debug.sh` or `build_release.sh`

## Running tests

Tests create temporary files under a scratch directory, resolved in this order:
1. `$FSI_TEST_ROOT` env var, if set
2. `/dev/shm` on Linux (RAM-backed, no SSD wear)
3. OS temp directory (default fallback, works with zero setup, wears down SSD)

For heavy fuzzing runs, set `FSI_TEST_ROOT` to a RAM disk:
- Linux: already defaults to `/dev/shm`, no action needed
- Windows: mount a RAM disk (e.g. via [ImDisk](https://sourceforge.net/projects/imdisk-toolkit/)) 
  and set `FSI_TEST_ROOT=R:\fsi_test`

## Third-Party Libraries
* [stb_image](https://github.com/nothings/stb) - Public Domain / MIT
* [clip.cpp](https://github.com/monatis/clip.cpp) - MIT License

Thanks to all the contributors of the libraries used in this project.