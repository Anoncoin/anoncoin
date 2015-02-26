How to use a bootstrap file to speed up blockchain synchronization
==================================================================

The Anoncoin client will normally download the blockchain from the network by synching with the other clients. Currently, the blockchain is about 400 Mb in size, and it will continue to grow with time. The first time that Anoncoin is run, full synchronization could take from several hours to more than a day. Fortunately, this process can be speeded up substantially by use of a bootstrap file.

To use a bootstrap file to speed up initial network synchronization, follow these simple steps.

Download the bootstrap file
---------------------------

The official Anoncoin bootstrap file can be downloaded either from the [anoncoin.net](https://anoncoin.net/downloads/bootstrap/) web site or by the i2p BitTorrent port i2psnark (filename: anoncoin.bootstrap.tar.xz). The compressed file is currently about 100 Mb in size. After uncompressing the file, locate the file bootstrap.dat and verify its SHA1 and MD5 checksums and GPG detached signature.

Copy the bootstrap file to the Anoncoin data directory
-----------------------------------------------------

Before importing the bootstrap.dat file, you will need to copy it to the correct location on your computer. If you have never used the Anoncoin software before, please start it once and then exit to create the required data directories. If the data directories already exist, please ensure that the Anoncoin software is not running. 

Place the bootstrap.dat file into the Anoncoin data folder. The location of this folder depends upon your platform.

**For Windows users:**

Open explorer, and type into the address bar:

	%APPDATA%\Anoncoin

**For OSX users:**

Open the Finder by pressing [shift] + [cmd] + [g] and then enter:

	~/Library/Application Support/Anoncoin/

**For Linux users:**

The directory is hidden in your User folder. Go to:

	~/.anoncoin/

Import the blockchain
---------------------

To import the bootstrap file, simply start the Anoncoin client software. When the wallet appears, you should see the text "Importing blocks from disk" in the lower left corner of the wallet. This process should take about 20 minutes, and after this, the client will begin to download the most recent blocks that were not in the bootstrap.dat file.

Is this safe?
-------------

Yes, the above method is safe. The download contains only the raw blockchain data and the client verifies this when it is imported.
