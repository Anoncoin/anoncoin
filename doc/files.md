The following files are found in the users Anoncoin directory:

* Windows XP: <code>C:\Documents and Settings\YourUserName\Application data\anoncoin</code>
* Windows Vista, Windows 7 and Windows 8: <code>C:\Users\YourUserName\Appdata\Roaming\anoncoin</code>
* Mac OS X: <code>~/Library/Application\ Support/Anoncoin/</code>
* Linux: <code>~/.anoncoin/</code>

Used in 0.9.4
-------------
* wallet.dat: personal wallet (BDB) with keys and transactions
* peers.dat: peer IP address database (custom format)
* anoncoin.conf: configuration file
* anoncoin.qss: Qt style sheet
* blocks/blk000??.dat: block data (custom, 128 MiB per file); since 0.8.0
* blocks/rev000??.dat; block undo data (custom); since 0.8.0
* blocks/index/*; block index (LevelDB); since 0.8.0
* chainstate/*; block chain state database (LevelDB); since 0.8.0
* database/*: BDB database environment; only used for wallet since 0.8.0

Used in 0.8.0
-------------
* wallet.dat: personal wallet (BDB) with keys and transactions
* peers.dat: peer IP address database (custom format)
* anoncoin.conf: configuration file
* blocks/blk000??.dat: block data (custom, 128 MiB per file); since 0.8.0
* blocks/rev000??.dat; block undo data (custom); since 0.8.0
* blocks/index/*; block index (LevelDB); since 0.8.0
* chainstate/*; block chain state database (LevelDB); since 0.8.0
* database/*: BDB database environment; only used for wallet since 0.8.0
