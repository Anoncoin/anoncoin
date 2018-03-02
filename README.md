The Anoncoin Project
====================

Overview
--------

Anoncoin (ANC) is a peer-to-peer digital cryptocurrency that focuses on privacy and anonymity for its users. Created in June 2013, it is the first and only currency to have built-in support for both the I2P darknet and Tor network that conceal the IP address of the user. Anoncoin will soon be implementing Zerocoin, which will allow users to make payments anonymously, without revealing their anoncoin public addresses.

Facts About Anonoin
--------------

* Launch date: June 6, 2013
* Proof of work algorithm: Scrypt
* Block generation: 3 minute block targets
* Block reward: 4.2 ANC for blocks until 42,000; 7 ANC until block 77,777; a 10 ANC bonus block for 77778; 5 ANC until block 306,600; and then halving of block rewards every 306,600 blocks (approximately every 638 days).
* Maximum number of coins: 3.1 million ANC
* Anonymity: Native support of the I2P and Tor darknets, Zerocoin in development
* Premine: 4200 ANC returned to the community

Building Anoncoin from source
-----------------------------

If all the required dependencies have already been built, compiling Anoncoin from source could be as simple as executing the following commands in the Anoncoin root directory:

    ./autogen.sh
    ./configure
    make

Detailed platform-specific instructions can be found in these files:

- [doc/README.md](doc/README.md)
- [doc/build-osx.md](doc/build-osx.md)
- [doc/build-msw.md](doc/build-msw.md)
- [doc/build-unix.md](doc/build-unix.md)

Pre-compiled Anoncoin binaries
------------------------------

Pre-compiled Anoncoin binaries can be downloaded either from the [anoncoin-binaries project](https://github.com/Anoncoin/anoncoin-binaries) or from the relevant links on the [Anoncoin wiki](https://wiki.anoncoin.net/Download). Please verify the gpg signatures and cehcksums before using these files.

Development process
-------------------

Developers work in their own trees, then submit pull requests when they think their feature or bug fix is ready. The master branch is built and tested regularly, but is not guaranteed to be completely stable. Tags are created to indicate new official, stable releases of Anoncoin. Feature branches are created when major new features are being developed, and the `develop` branch is used for all other routine development. Any branch besides `master` should be used with extreme caution. Detailed information concerning the Anoncoin code base can be found in the [developer documentation](http://anoncoin.github.io/anoncoin).

For more information
--------------------
**Anoncoin website:** [anoncoin.net](https://anoncoin.net/)<br />
**Anoncoin wiki:** [wiki.anoncoin.net](https://wiki.anoncoin.net/)<br />
**email:** [contact@anoncoin.net](mailto:contact@anoncoin.net)<br />
**IRC:** #anoncoin (I2P: localhost port 6668 / Freenode: irc.freenode.net port 6667)<br />
**Twitter:** [AnoncoinNews](https://twitter.com/AnoncoinNews), [AnoncoinProject](https://twitter.com/AnoncoinProject)
