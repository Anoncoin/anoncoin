### Bootstrap the Blockchain Synchronization

Normally the Anoncoin client will download the transaction and network information, called the blockchain, from the network by syncing with the other clients. This can be a process that can take multiple days as the [Anoncoin block chain](https://blockchain.info/charts/blocks-size) has grown to more than 15 gigabytes, and is growing almost a gigabyte every month. Luckily there is a safe and fast way to speed up this process. We’ll show you how to bootstrap your blockchain to bring your client up to speed in just a few simple steps.

### Requirements

A fresh install of the Anoncoin client software.

### Download the blockchain via Bittorent

Anoncoin core developers, offer an [torrent file]for bootstrapping purposes. Bittorrent is a protocol that speeds up the downloading of large files by using the other clients in the network. Examples of free and safe open-source clients are [Deluge](http://deluge-torrent.org/) or [QBittorent](http://www.qbittorrent.org/). A guide to installing and configuring the torrent clients can be found [here](http://dev.deluge-torrent.org/wiki/UserGuide) for Deluge and [here](http://qbforums.shiki.hu/) for QBittorent. A further in-depth tutorial on Bittorent can be found [here](http://www.howtogeek.com/howto/31846/bittorrent-for-beginners-how-get-started-downloading-torrents/).

With the client installed we’ll proceed to download the blockchain torrent file. Use the following magnet link:

	magnet:?xt=urn: TODO: [define here]

 or go to TODO: [define here] for a signed magnet link. Alternately you can use the [.torrent file] TODO: [define here]  found in our Github repository..

![Fig1](img/bootstrap1.png)

The download page should look like this, with a countdown to the download. If it does not work click the direct download link.

The torrent client installed will recognize the download of the torrent file. Save the bootstrap.dat file to a folder you use for downloads. The image below shows the torrent download in QBittorent, with current speed and ETA highlighted.

![Fig2](img/bootstrap2.png)

### Download the block chain directly from official repositories
The Bittorent version, see above, of the block chain download is refreshed more often than the direct download available. If Bittorent is blocked on your network then you can use the direct download method. Be sure to only use official repositories as the link displayed below. This download will only update the client to March 2013.

Click [here](TODO: [define here]) to download or copy and paste the link below.

	TODO: [define here]

The download page should look like this, with a countdown to the download. If it does not work directly click the download. Save the file to a folder you use for downloads.
![Fig3](img/bootstrap3.png)

### Importing the blockchain
Exit the Anoncoin Client software if you have it running. Be sure not to have an actively used wallet in use. We are going to copy the download of the blockchain to the Anoncoin client data directory. You should run the client software at least once so it can generate the data directory. Copy the downloaded bootstrap.dat file into the Anoncoin data folder.

**For Windows users:**
Open explorer, and type into the address bar:

	%APPDATA%\Anoncoin

This will open up the data folder. It should look like the image below. Copy over the bootstrap.dat from your download folder to this directory.
![Fig4](img/bootstrap4.png)

**For OSX users:**
Open Finder by pressing Press [shift] + [cmd] + [g] and enter:

	~/Library/Application Support/Anoncoin/

**For Linux users:**
The directory is hidden in your User folder. Go to:

	~/.anoncoin/

### Importing the blockchain
Now start the Anoncoin client software. It should show "Importing blocks from disk" like the image below.
![Fig5](img/bootstrap5.png)

Wait until the import finishes. The client will download the last days not covered by the import. Congratulations you have successfully imported the blockchain!

### Is this safe?

Yes, the above method is safe. The download contains only raw block chain data and the client verifies this on import. Do not download the blockchain from unofficial sources, especially if they provide `*.rev` and `*.sst` files. These files are not verified and can contain malicious edits.
