#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mhash.h>
#include <list>
using namespace std;

#define BUFFERSIZE 262144
//#define USESTORE // Works
//#define USEDEFLATE // Works
//#define USEBZIP2 // Works
//#define USELZMA // I don't think this one works. 7zip doesn't like it.

#if defined(USESTORE)
	// None needed.
#elif defined(USEDEFLATE)
#	include <zlib.h>
#	ifndef COMPRESSIONLEVEL
#		define COMPRESSIONLEVEL Z_DEFAULT_COMPRESSION
#	endif
#elif defined(USEBZIP2)
#	include <bzlib.h>
#	ifndef COMPRESSIONLEVEL
#		define COMPRESSIONLEVEL 6
#	endif
#elif defined(USELZMA)
#	include <lzma.h>
#	ifndef COMPRESSIONLEVEL
#		define COMPRESSIONLEVEL LZMA_PRESET_DEFAULT
#	endif
#else
#	warn "No compression method selected, defaulting to STORE."
#	define USESTORE
#endif

// https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT

struct __attribute__((packed)) LocalHeader {
	uint32_t signature;
	uint16_t version;
	uint16_t flags;
	uint16_t method;
	uint16_t mtime;
	uint16_t mdate;
	uint32_t crc;
	uint32_t csize;
	uint32_t usize;
	uint16_t fnamelen;
	uint16_t extralen;
};
struct __attribute__((packed)) DataDescriptor {
	uint32_t signature;
	uint32_t crc;
	uint32_t csize;
	uint32_t usize;
};
struct __attribute__((packed)) Zip64DataDescriptor {
	uint32_t signature;
	uint32_t crc;
	uint64_t csize;
	uint64_t usize;
};
struct __attribute__((packed)) DirectoryHeader {
	uint32_t signature;
	uint16_t cversion;
	uint16_t eversion;
	uint16_t flags;
	uint16_t method;
	uint16_t mtime;
	uint16_t mdate;
	uint32_t crc;
	uint32_t csize;
	uint32_t usize;
	uint16_t fnamelen;
	uint16_t extralen;
	uint16_t commentlen;
	uint16_t disknum;
	uint16_t iattribs;
	uint32_t eattribs;
	uint32_t offset;
};
struct __attribute__((packed)) Zip64Info {
	uint16_t signature;
	uint16_t size;
	uint64_t usize;
	uint64_t csize;
	uint64_t offset;
	uint32_t disk;
};
struct __attribute__((packed)) Zip64DirectoryRecord {
	uint32_t signature;
	uint64_t drsize;
	uint16_t cversion;
	uint16_t eversion;
	uint32_t disknum;
	uint32_t diskdir;
	uint64_t diskcount;
	uint64_t direntries;
	uint64_t dirsize;
	uint64_t diroffset;
};
struct __attribute__((packed)) Zip64DirectoryLocator {
	uint32_t signature;
	uint32_t dirdisk;
	uint64_t diroffset;
	uint32_t diskcount;
};
struct __attribute__((packed)) DirectoryEnd {
	uint32_t signature;
	uint16_t disknum;
	uint16_t diskdir;
	uint16_t diskcount;
	uint16_t direntries;
	uint32_t dirsize;
	uint32_t diroffset;
	uint16_t zipcommentlen;
};

struct ZippedFile {
	const char* name;
	LocalHeader header;
	size_t csize, usize;

	ZippedFile(const char* n) {
		name = n;
	}
};


uint16_t dostime(long time) {
	struct tm* btime = localtime(&time);
	return (btime->tm_hour << 11) | (btime->tm_min << 5) | (btime->tm_sec / 2);
}
uint16_t dosdate(long time) {
	struct tm* btime = localtime(&time);
	return ((btime->tm_year-80) << 9) | ((btime->tm_mon+1) << 5) | (btime->tm_mday);
}

ZippedFile* do_file(const char* name) {
	ZippedFile* file = new ZippedFile(name);

	int fd = open(name, O_RDONLY);
	if (fd < 0) return NULL;

	struct stat st;
	fstat(fd, &st);



	LocalHeader header;
	header.signature = 0x04034b50;
	#if defined(USESTORE)
		header.version = 45; // 10 = default, 20 = deflate, 21 = deflate64, 45 = zip64, 46 = bzip2, 63 = lzma
		header.flags = 0b1000; // bit 1&2 describe compression, others unimportant
		header.method = 0; // 0 = store, 8 = deflate, 9 = deflate64, 12=bzip2, 14 = lzma
	#elif defined(USEDEFLATE)
		header.version = 45;
		header.flags = 0b1000; // 00x = normal compression
		header.method = 8;
	#elif defined(USEBZIP2)
		header.version = 46;
		header.flags = 0b1000; // bit 1&2 unused
		header.method = 12;
	#elif defined(USELZMA)
		header.version = 63;
		header.flags = 0b1000; // 1x = uses EOS marker to indicate end-of-stream
		header.method = 14;
	#endif
	header.mtime = dostime(st.st_mtime);
	header.mdate = dosdate(st.st_mtime);
	header.crc = 0;
	header.csize = 0;
	header.usize = 0; // The spec says this should be left 0. Actual software varies.
	header.fnamelen = strlen(name);
	header.extralen = 4;

	fwrite(&header, 1, sizeof(header), stdout);
	fwrite(name, 1, strlen(name), stdout);
	
	Zip64Info zi;
	zi.signature = 0x0001;
	zi.size = 0;

	fwrite(&zi, 1, 4, stdout);



	MHASH mh = mhash_init(MHASH_CRC32B);

	uint8_t inbuf[BUFFERSIZE];
	#ifndef USESTORE
	uint8_t outbuf[BUFFERSIZE];
	#endif
	size_t len, total = 0;

	#if defined(USESTORE)
	{
		while ((len = read(fd, inbuf, BUFFERSIZE)) > 0) {
			mhash(mh, inbuf, len);
			fwrite(inbuf, 1, len, stdout);
			total += len;
		}
	}
	#elif defined(USEDEFLATE)
	{
		int flush, ret;
		z_stream zp;
		zp.zalloc = Z_NULL;
		zp.zfree = Z_NULL;
		zp.opaque = Z_NULL;
		ret = deflateInit2(&zp, COMPRESSIONLEVEL, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
		assert(ret == Z_OK);

		do {
			len = read(fd, inbuf, BUFFERSIZE);
			flush = (len == BUFFERSIZE) ? Z_NO_FLUSH : Z_FINISH;

			mhash(mh, inbuf, len);

			zp.avail_in = len;
			zp.next_in = inbuf;

			do {
				zp.avail_out = BUFFERSIZE;
				zp.next_out = outbuf;

				deflate(&zp, flush);
				len = BUFFERSIZE - zp.avail_out;

				fwrite(outbuf, 1, len, stdout);
				total += len;
			} while (zp.avail_out == 0);
		} while (flush != Z_FINISH);

		deflateEnd(&zp);
	}
	#elif defined(USEBZIP2)
	{
		int action, ret;
		bz_stream zp;
		zp.bzalloc = NULL;
		zp.bzfree = NULL;
		zp.opaque = NULL;
		ret = BZ2_bzCompressInit(&zp, COMPRESSIONLEVEL, 0, 0);
		assert(ret == BZ_OK);

		do {
			len = read(fd, inbuf, BUFFERSIZE);
			action = (len == BUFFERSIZE) ? BZ_RUN : BZ_FINISH;

			mhash(mh, inbuf, len);

			zp.avail_in = len;
			zp.next_in = (char*)inbuf;

			do {
				zp.avail_out = BUFFERSIZE;
				zp.next_out = (char*)outbuf;

				ret = BZ2_bzCompress(&zp, action);
				assert(ret == BZ_RUN_OK || ret == BZ_FINISH_OK || ret == BZ_STREAM_END);
				len = BUFFERSIZE - zp.avail_out;

				fwrite(outbuf, 1, len, stdout);
				total += len;
			} while (zp.avail_out == 0 && ret != BZ_STREAM_END);
		} while (action != BZ_FINISH);

		BZ2_bzCompressEnd(&zp);
	}
	#elif defined(USELZMA)
	{
		int ret;
		lzma_action action = LZMA_RUN;

		lzma_options_lzma opt;
		ret = lzma_lzma_preset(&opt, COMPRESSIONLEVEL);
		assert(ret == LZMA_OK);

		lzma_stream zp = LZMA_STREAM_INIT;
		ret = lzma_alone_encoder(&zp, &opt);
		if (ret != LZMA_OK) {
			fprintf(stderr, "lzma_stream_encoder returned %d\n", ret);
			switch (ret) {
			case LZMA_MEM_ERROR:
				fprintf(stderr, "LZMA_MEM_ERROR\n");
				break;
			case LZMA_OPTIONS_ERROR:
				fprintf(stderr, "LZMA_OPTIONS_ERROR\n");
				break;
			case LZMA_UNSUPPORTED_CHECK:
				fprintf(stderr, "LZMA_UNSUPPORTED_CHECK\n");
				break;
			}
			exit(3);
		}

		do {
			len = read(fd, inbuf, BUFFERSIZE);
			action = (len == BUFFERSIZE) ? LZMA_RUN : LZMA_FINISH;

			mhash(mh, inbuf, len);

			zp.avail_in = len;
			zp.next_in = inbuf;

			do {
				zp.avail_out = BUFFERSIZE;
				zp.next_out = outbuf;

				ret = lzma_code(&zp, action);
				assert(ret == LZMA_OK || ret == LZMA_STREAM_END);
				len = BUFFERSIZE - zp.avail_out;

				fwrite(outbuf, 1, len, stdout);
				total += len;
			} while (zp.avail_out == 0 && ret != LZMA_STREAM_END);
		} while (action == LZMA_RUN);

		lzma_end(&zp);
	}
	#endif

	mhash_deinit(mh, &header.crc);
	header.usize = st.st_size < 0xFFFFFFFF ? st.st_size : 0xFFFFFFFF;
	header.csize = total < 0xFFFFFFFF ? total : 0xFFFFFFFF;


	Zip64DataDescriptor dd;
	dd.signature = 0x08074b50;
	dd.crc = header.crc;
	dd.csize = total;
	dd.usize = st.st_size;
	fwrite(&dd, sizeof(dd), 1, stdout);

	file->csize = total;
	file->usize = st.st_size;
	file->header = header;
	return file;
}

int main(int argc, char* argv[]) {
	list<ZippedFile*> files;

	for (int f = 1; f < argc; f++) {
		ZippedFile* file = do_file(argv[f]);
		if (file == NULL) {
			fprintf(stderr, "Error zipping '%s': %m\n", argv[f]);
			return 2;
		}
		files.push_back(file);
	}

	size_t offset = 0, dirsize = 0;
	for (auto i = files.begin(); i != files.end(); i++) {
		LocalHeader* lh = &((*i)->header);

		DirectoryHeader dh;
		dh.signature = 0x02014b50;
		dh.cversion = 3; // UNIX
		memcpy(&(dh.eversion), &(lh->version), sizeof(LocalHeader)-4);
		dh.commentlen = 0;
		dh.disknum = 0;
		dh.iattribs = 0;
		dh.eattribs = 0;
		dh.offset = offset < 0xFFFFFFFF ? offset : 0xFFFFFFFF;
		
		Zip64Info zi;
		zi.signature = 0x0001;
		zi.size = 0;
		zi.usize = (*i)->usize;
		zi.csize = (*i)->csize;
		zi.offset = offset;
		
		if (dh.offset == 0xFFFFFFFF) zi.size = 24;
		else if (dh.csize == 0xFFFFFFFF) zi.size = 16;
		else if (dh.usize == 0xFFFFFFFF) zi.size = 8;
		dh.extralen = zi.size + 4;

		fwrite(&dh, 1, sizeof(dh), stdout);
		fwrite((*i)->name, 1, dh.fnamelen, stdout);
		fwrite(&zi, 1, dh.extralen, stdout);

		offset += sizeof(LocalHeader) + dh.fnamelen + 4 + dh.csize + sizeof(Zip64DataDescriptor);
		dirsize += sizeof(DirectoryHeader) + dh.fnamelen + dh.extralen;
		
		delete *i;
	}

	Zip64DirectoryRecord dr;
	dr.signature = 0x06064b50;
	dr.drsize = sizeof(dr) - 12;
	dr.cversion = 3; // Spec suggests this should be 3 = unix, 7Zip sets it matching eversion.
	#if defined(USESTORE) || defined(USEDEFLATE)
		dr.eversion = 45; // 45 = zip64, 46 = bzip2, 63 = lzma
	#elif defined(USEBZIP2)
		dr.eversion = 46;
	#elif defined(USELZMA)
		dr.eversion = 63;
	#endif
	dr.disknum = 0;
	dr.diskdir = 0;
	dr.diskcount = argc-1;
	dr.direntries = argc-1;
	dr.dirsize = dirsize;
	dr.diroffset = offset;

	fwrite(&dr, 1, sizeof(dr), stdout);
	
	
	Zip64DirectoryLocator dl;
	dl.signature = 0x07064b50;
	dl.dirdisk = 0;
	dl.diroffset = offset+dirsize;
	dl.diskcount = 1;
	
	fwrite(&dl, 1, sizeof(dl), stdout);


	DirectoryEnd de;
	de.signature = 0x06054b50;
	de.disknum = 0;
	de.diskdir = 0;
	de.diskcount = argc-1;
	de.direntries = argc-1;
	de.dirsize = dirsize < 0xFFFFFFFF ? dirsize : 0xFFFFFFFF;
	de.diroffset = offset < 0xFFFFFFFF ? offset : 0xFFFFFFFF;
	de.zipcommentlen = 0;
	fwrite(&de, 1, sizeof(de), stdout);


	fflush(stdout);
	fclose(stdout);
	return 0;
}
