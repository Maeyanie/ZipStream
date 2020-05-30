all: zipstream-store zipstream-deflate zipstream-bzip2 zipstream-lzma

zipstream-store: zipstream.cpp
	$(CXX) -O2 -Wall -o zipstream-store -DUSESTORE zipstream.cpp -lmhash

zipstream-deflate: zipstream.cpp
	$(CXX) -O2 -Wall -o zipstream-deflate -DUSEDEFLATE zipstream.cpp -lmhash -lz

zipstream-bzip2: zipstream.cpp
	$(CXX) -O2 -Wall -o zipstream-bzip2 -DUSEBZIP2 zipstream.cpp -lmhash -lbz2

zipstream-lzma: zipstream.cpp
	$(CXX) -O2 -Wall -o zipstream-lzma -DUSELZMA zipstream.cpp -lmhash -llzma
