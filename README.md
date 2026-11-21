## License
Copyright (C) 2026 Simon Feiler

## General Information
Currently, the indexer only works for Linux systems, but many components were design platform independent so Windows/Mac support will come soon.

I designed this indexer to work mainly for images, so the cmd version described below can be used to search for images via prompt or reference images.

There is also a server version where one can search for file data via logical and relational queries, but this is currently in development and can therefor not be used yet. I'm also working on a tauri frontend.
## Building the indexer
For the indexer cmd version to work, one has to clone the MemObjStorage library first.
Make sure to clone it into the same directory as the indexer.
https://github.com/sifeiler/MemObjStorage

Please be aware that the following downloads .gguf files from hugging face.

After that, run `build_debug.sh` or `build_release.sh`

## How to use it
Open a terminal, go to your build directory and run the following command:  
`./indexer <path-to-storage-dir (i.e. /tmp/storage)> <root-directory-where-indexing-starts> <max-file-count>`  

A full example would be (with 1000 files max):  
`./indexer /tmp/storage /home/images 1000`  

The storage directory has to exist.

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
* [mongoose](https://github.com/cesanta/mongoose/) - GNU General Public License version 2
* [stb_image](https://github.com/nothings/stb) - Public Domain / MIT
* [clip.cpp](https://github.com/monatis/clip.cpp) - MIT License
* [libb64](https://github.com/libb64/libb64) - Public Domain
* [cJSON](https://github.com/davegamble/cjson) - MIT License
* [Unity](https://github.com/throwtheswitch/unity) - MIT License

Thanks to all the contributors of the libraries used in this project.