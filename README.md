## Building the indexer
For the indexer to work, one has to clone the MemObjStorage library first.
Make sure to clone it into the same directory as the indexer.
https://github.com/sifeiler/MemObjStorage

After that, run `build_debug.sh` or `build_release.sh`

## How to use it
Open a terminal, go to your build directory and run the following command:  
`./indexer <path-to-storage-file (i.e. /tmp/storage.db)> <root-directory-where-indexing-starts> <max-file-count>`  

A full example would be (with 1000 files max):  
`./indexer /tmp/storage.db /home/simon/images 1000`  

Wait until you see the output:  
`Baseline indexing: done`  

Now you can use the vector search and search for your images:  

=== Interactive Vector Search ===  
Usage:  
  [threshold] text: <prompt>   (e.g., '0.72 text: a red car')  
  [threshold] image: <path>    (e.g., '0.30 image: /path/img.jpg')  
  (Defaults: Image threshold = 0.35, Text threshold = 0.75)  

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