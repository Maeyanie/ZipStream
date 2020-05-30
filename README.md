# ZipStream
Streams a list of files into a compression-configurable .zip file with no lengthy pre-building step needed.

Looking at you, Google Drive and Sync.

Due to this, it also uses no extra server hard drive space to pre-build. It can also optionally compress with deflate, bzip2, or lzma compression algorithms, though I couldn't find any decompression software supporting it to test the latter.

No Zip64 support yet, but if anyone actually uses this for anything, feel free to submit an Issue requesting it and I can add that. It doesn't look too difficult, but I only wrote this as a proof-of-concept so didn't bother.

The Makefile is set up for compiling on my system (Fedora Linux), and may need slight adjustments for other systems.
