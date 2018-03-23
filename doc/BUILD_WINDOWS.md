WINDOWS BUILD NOTES
===================


Compilers Supported
-------------------
TODO: What works? Only barely started updating this,  there are many more dependencies and details to
      document here, nor do we any longer include source to build with the executables.
      This document needs to be upgraded to the v9.5 Anoncoin Core level, it is very outdated.

      The boost library requires two or three additional components which you must add over what most
      other coins require.

Note: Releases are cross-compiled using mingw running on Linux.  That is one possible way to do it.
      Another is to follow the thread on bitcointalk.org, specifically setup for building on Windows.


Dependencies
------------
Libraries you need to download separately and build:  This is no longer a requirement if you use the depends directory
to build your libraries before building the main Anoncoin software, it is setup for that and takes care of the download
process as well.

	name            default path               download
	--------------------------------------------------------------------------------------------------------------------
	OpenSSL         \openssl-1.0.1j            http://www.openssl.org/source/
	Berkeley DB     \db-4.8.30.NC              http://www.oracle.com/technology/software/products/berkeley-db/index.html
	Boost           \boost-1.57.0              http://www.boost.org/users/download/
	miniupnpc       \miniupnpc-1.9.20140701    http://miniupnp.tuxfamily.org/files/

Their licenses:

	OpenSSL        Old BSD license with the problematic advertising requirement
	Berkeley DB    New BSD license with additional requirement that linked software must be free open source
	Boost          MIT-like license
	miniupnpc      New (3-clause) BSD license

Versions used in this release:

	OpenSSL      openssl-1.0.1j.tar.gz
	Berkeley DB  db-4.8.30.NC.tar.gz
	Boost        boost_1_57_0.tar.bz2
	miniupnpc    miniupnpc-1.9.20140701.tar.gz


Most of the following details are incorrect or not needed.

OpenSSL
-------
MSYS shell:

un-tar sources with MSYS 'tar xfz' to avoid issue with symlinks (OpenSSL ticket 2377)
change 'MAKE' env. variable from 'C:\MinGW32\bin\mingw32-make.exe' to '/c/MinGW32/bin/mingw32-make.exe'

	cd /c/openssl-1.0.1c-mgw
	./config
	make

Berkeley DB
-----------
MSYS shell:

	cd /c/db-4.8.30.NC-mgw/build_unix
	sh ../dist/configure --enable-mingw --enable-cxx
	make

Boost
-----
MSYS shell:

	downloaded boost jam 3.1.18
	cd \boost-1.50.0-mgw
	bjam toolset=gcc --build-type=complete stage

MiniUPnPc
---------
UPnP support is optional, make with `USE_UPNP=` to disable it.

MSYS shell:

	cd /c/miniupnpc-1.6-mgw
	make -f Makefile.mingw
	mkdir miniupnpc
	cp *.h miniupnpc/

Anoncoin
-------
MSYS shell:

	cd \anoncoin
	sh autogen.sh
	sh configure
	mingw32-make
	strip bitcoind.exe
