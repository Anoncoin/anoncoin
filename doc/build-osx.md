Mac OS X Build Instructions and Notes
====================================
The commands in this guide should be executed in a Terminal application.
The built-in one is located in `/Applications/Utilities/Terminal.app`.

Preparation
-----------
Install the OS X command line tools:

`xcode-select --install`

When the popup appears, click `Install`.

Then install [Homebrew](https://brew.sh).

Dependencies
----------------------

    brew install automake berkeley-db4 libtool boost miniupnpc openssl pkg-config protobuf python3 qt libevent

See [dependencies.md](dependencies.md) for a complete overview.

If you want to build the disk image with `make deploy` (.dmg / optional), you need RSVG

    brew install librsvg

NOTE: Building with Qt4 is still supported, however, could result in a broken UI. Building with Qt5 is recommended.

Get Anoncoin Source Code
-----------
Clone the anoncoin source code and cd into `anoncoin`

```bash
git clone https://github.com/Anoncoin/anoncoin
cd anoncoin
```

Install Berkeley DB
-----------
It is recommended to use Berkeley DB 4.8. If you have to build it yourself,
you can use [the installation script included in contrib/](/contrib/install_db4.sh)
like so

```bash
./contrib/install_db4.sh .
```

from the root of the repository.

**Note**: You only need Berkeley DB if the wallet is enabled (see the section *Disable-Wallet mode* below).

OpenSSL Config
---
In order for Anoncoin to find the correct OpenSSL lib, some flags must be set before compiling:

```bash
export LDFLAGS=-L/usr/local/opt/openssl/lib
export CPPFLAGS=-I/usr/local/opt/openssl/include
```

Build Anoncoin Core
------------------------

1.  Build anoncoin-core:

    Configure and build the headless anoncoin binaries as well as the GUI (if Qt is found).

    You can disable the GUI build by passing `--without-gui` to configure.

```bash
./autogen.sh
./configure
make
```

2.  It is recommended to build and run the unit tests:

```bash
make check
```

4.  You can also create a .dmg that contains the .app bundle (optional):

```bash
make deploy
```

Running
-------

Anoncoin Core is now available at `./src/anoncoind`

Before running, it's recommended you create an RPC configuration file.

    echo -e "rpcuser=anoncoinrpc\nrpcpassword=$(xxd -l 16 -p /dev/urandom)" > "/Users/${USER}/Library/Application Support/Anoncoin/anoncoin.conf"

    chmod 600 "/Users/${USER}/Library/Application Support/Anoncoin/anoncoin.conf"

The first time you run anoncoind, it will start downloading the blockchain. This process could take several hours.

You can monitor the download process by looking at the debug.log file:

    tail -f $HOME/Library/Application\ Support/Anoncoin/debug.log

Other commands:
-------

    ./src/anoncoind -daemon # Starts the anoncoin daemon.
    ./src/anoncoin-cli --help # Outputs a list of command-line options.
    ./src/anoncoin-cli help # Outputs a list of RPC commands when the daemon is running.

Notes
-----

* Tested on OS X 10.8 through 10.13 on 64-bit Intel processors only.

* Building with downloaded Qt binaries is not officially supported. See the notes in [#7714](https://github.com/bitcoin/bitcoin/issues/7714)
