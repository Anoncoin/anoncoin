Mac OS X Build Instructions
===========================

This guide describes how to build Anoncoin for OSX.

Notes
-----

* This was tested using OS X 10.9 on a 64-bit Intel processor. Older OSX releases or 32-bit processors are no longer supported.

* All of the commands should be executed in the terminal application. The built-in one is located in `/Applications/Utilities`.

* You need to install [XCode](https://developer.apple.com/xcode/) with all the options checked. Furthermore, you will need to install the XCode command line tools by executing the command <code>xcode-select --install</code> in the terminal.
    
* If you are building Anoncoin for your own use, the homebrew instructions below are probably the easiest and fastest. If you encounter any errors when installing anoncoin, the release build instructions are your best option.

* The blockchain, the user <code>wallet.dat</code> file, and the <code>anoncoin.conf</code> configuration file are all located in the directory <code>~Library/Application\ Support/Anoncoin</code>.

Homebrew instructions
------------------------

If homebrew is not already installed, install it using the following command:

    ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"

Start by installing the dependencies that are required for Anoncoin

    brew install autoconf automake libtool boost miniupnpc openssl pkg-config protobuf qt

The package berkeley-db4 is required, but the homebrew package has been broken for some time. Running the following command takes you into brew's interactive mode, which allows you to configure, make, and install berkeley-db4 by hand:

    brew install https://raw.github.com/mxcl/homebrew/master/Library/Formula/berkeley-db4.rb -–without-java 

The following commands are run in brew's interactive mode (where all text including, and to the left of, $ is ignored):

    /private/tmp/berkeley-db4-UGpd0O/db-4.8.30 $ cd ..
    /private/tmp/berkeley-db4-UGpd0O $ db-4.8.30/dist/configure --prefix=/usr/local/Cellar/berkeley-db4/4.8.30 --mandir=/usr/local/Cellar/berkeley-db4/4.8.30/share/man --enable-cxx
    /private/tmp/berkeley-db4-UGpd0O $ make
    /private/tmp/berkeley-db4-UGpd0O $ make install
    /private/tmp/berkeley-db4-UGpd0O $ exit

After exiting, you will get a warning that the install is keg-only, which means it wasn't symlinked to <code>/usr/local</code>.  You don't need it to link it to build anoncoin, but if you want to, execute this command

    brew --force link berkeley-db4

Now that the dependencies are build, we can start building Anoncoin usins the github source code. Execute the following commands in the terminal

    git clone https://github.com/Anoncoin/anoncoin.git
    cd anoncoin
	./autogen.sh
    ./configure
    make
    make install
    make deploy

This will create the binaries <code>src/anoncoind</code>, <code>src/anoncoin-cli</code>, <code>src/qt/anoncoin-qt</code>, the application <code>Anoncoin.app</code>, and the <code>Anoncoin.dmg</code> diskimage.

Lastly, it is a good idea to check that everything is working correctly by running the test suite

    make check

Release build instructions
--------------------------

First, download the Anoncoin code, and create the make files using the commands

    git clone https://github.com/Anoncoin/anoncoin.git
    cd anoncoin
    ./autogen.sh

Next, change to the directory <code>depends</code> to start the process of building the required dependencies

    cd depends
    ./config.guess

This last command should output a triplet that looks something like this: <code>x86_64-apple-darwin13.4.0</code>. Enter (changing the triplet if necessary)

    make HOST=x86_64-apple-darwin13.4.0

The dependencies should download and build. If there are no errors, continue to building the anoncoin binaries:

    cd ..
    ./configure --prefix=$PWD/depends/x86_64-apple-darwin13.4.0
    make HOST=x86_64-apple-darwin13.4.0
    make install
    make deploy

This will creat the binaries <code>src/anoncoind</code>, <code>src/anoncoin-cli</code>, <code>src/qt/anoncoin-qt</code>, the application <code>Anoncoin.app</code> and the <code>Anoncoin.dmg</code> diskimage.

Lastly, it is a good idea to check that everything is working correctly by running the test suite

    make check

Running
-------

If on first execution you are updating from 0.8.x to 0.9.x, it will be necessary to rebuild the block chain by executing <code>anoncoind</code> or <code>anoncoin-qt</code> with the <code>reindex</code> option

    src/qt/anoncoin-qt -reindex
    src/anoncoind -reindex

The following commands should get you started:

* <code>anoncoin-qt</code> to start to Anoncoin wallet graphical interface.
* <code>anoncoind -daemon</code> to start the Anoncoin daemon.
* <code>anoncoin-cli --help</code> for a list of command-line options.
* <code>bitcoin-cli help</code>  to get a list of RPC commands when the daemon is running.
