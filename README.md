# ZipStream
Streams a list of files into a compression-configurable .zip file with no lengthy pre-building step needed.

Looking at you, Google Drive, Sync, and similar sites.

Due to this, it also uses no extra server hard drive space to pre-build. It can also optionally compress with deflate, bzip2, or lzma compression algorithms, though the latter is probably broken right now.

The Makefile is set up for compiling on my system (Fedora Linux), and may need slight adjustments for other systems.
