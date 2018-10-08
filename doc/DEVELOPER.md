# Developer Page

[Installation](./INSTALLATION.md) |
**Developers** |
[Roadmap](./ROADMAP.md) |
[Wallet](./WALLET.md)

Getting Involved
----------------
Here are some suggestions on how to get involved with Anoncoin:

| ITEM | DESCRIPTION |
| --- | --- |
| Join the community | Make sure you are part of the [Anoncoin Slack Channel](https://anoncoin.slack.com).  The #anoncoin-dev channel is specifically for developers' questions.  Subscribe to the Anoncoin subreddit. |
| Install Anoncoin | Install Anoncoin onto your development server.  Add [Github issues](https://github.com/Anoncoin/anoncoin/issues) or [submit pull requests](https://github.com/Anoncoin/anoncoin/pulls) with any issues you may find.
| Update documentation | Update [documentation](https://github.com/Anoncoin/anoncoin/tree/master/doc) if you find anything difficult to understand. |
| Translate | Translate the language files in the project.  [More information here](./TRANSLATION.md). |
| Fix an issue | Look at the open [Github issues](https://github.com/Anoncoin/anoncoin/issues) and find one that you are suited to fix.  Perhaps start with the issues tagged "[good first issue](https://github.com/Anoncoin/anoncoin/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22)"
| Add a feature | Add a new feature to the code.  Look at the [Anoncoin roadmap]() and [project backlog](https://github.com/Anoncoin/anoncoin/projects/1) for inspiration! |


Development Process
-------------------

Developing for Anoncoin is encouraged!  We follow a process which allows for flexibility and engagement from the development community, with the required structure and audits that is needed to ensure that we stick to the key security and privacy mandates that Anoncoin has.

| ITEM | DESCRIPTION |
| --- | --- |
| Fork the code | [Fork the Anoncoin code](https://github.com/Anoncoin/anoncoin) into your own branch to start development. |
| Prepare pull request template | The request description should give overview of the feature, a link to the Github issue or other discussion about the fix, notes for core developers to help reviewing the pull request, and notes on any changes to the network or to the build process. |
| Submit pull request | Once your code is developed and tested, [submit a pull request](https://github.com/Anoncoin/anoncoin/pulls) with the bug fix or feature enhancement.  Make sure to contain your code changes to those that are directly needed to achieve the fix - don't bundle different code changes together.
| Code review | The core developers will perform a code review on the pull request and will either suggest changes or accept the pull request |


Source Code Branches
--------------------

The following is the methodology for branches and releases:

| BRANCH | DESCRIPTION |
| --- | --- |
| `master` | Latest tested and working code. |
| `develop` | Alpha and beta quality code that contains large features that have not yet been deemed ready to push to master.  Use with extreme caution. |
| `tags/v0.x.y.z` | Official releases that master nodes and wallets can us.|
| `(other)` | Branches that core developers made that are not intended for public use.  In general, these should be forks rather than branches.

Unit Tests
----------

Compiling/running unit tests
----------------------------

Unit tests will be automatically compiled if dependencies were met in `configure` and tests weren't explicitly disabled.

After configuring, they can be run explicity:

```
make check
```

To run the anoncoind tests manually:

```
./src/test/test_anoncoin .
```

To add more anoncoind tests, add `BOOST_AUTO_TEST_CASE` functions to the existing
.cpp files in the test/ directory or add new .cpp files that
implement new `BOOST_AUTO_TEST_SUITE` sections.

To run the anoncoin-qt tests manually:

```
src/qt/test/anoncoin-qt_test
```

To add more anoncoin-qt tests, add them to the `src/qt/test/` directory and the `src/qt/test/test_main.cpp` file.

Important Files
---------------

The following are important files in the Anoncoin Data Directory and their uses:

| FILE NAME | DESCRIPTION |
| --- | --- |
| `wallet.dat` | Personal wallet (stored in BDB) with keys and transactions |
| `peers.dat` | Peer IP address database (custom format) |
| `anoncoin.conf` | Anoncoin configuration file |
| `anoncoin.qss` | Qt style sheet for the Anoncoin wallet UI |
| `blocks/blk000*.dat` | Block data (custom, 128 MiB per file). |
| `blocks/rev000*.dat` | Block undo data (custom) |
| `blocks/index/*` | Block index (LevelDB) |
| `chainstate/*` | Block chain state database (LevelDB) |
| `database/*` | BDB database environment; used for wallet since 0.8.0 |
| `debug.log` | The debug log file.  Tail this log file to read system log messages when the Anoncoin node is running |
| `db.log` | ??? |
| `anoncoin.pid` | The .lock file for processes to track whether Anoncoin is running |
| `bootstrap.dat` | A bootstrap file which accelerated the initialization of the blockchain file system. |

Additional Resources
--------------------

| ITEM | DESCRIPTION |
| --- | --- |
| [Code Guidelines ](./CODE_GUIDELINES.md) | Coding standards and [generated code documentation](http://anoncoin.github.io/anoncoin) for the class files in the source code. |
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Translation Process](translation_process.md)
- [Unit Tests](unit-tests.md)
- [Tor Support](tor.md)
- [Assets Attribution](assets-attribution.md)

Need Help?
----------

* See the documentation on the [Anoncoin wiki](https://wiki.anoncoin.net).
* Ask for help on the IRC #anoncoin channel on Freenode (irc.freenode.net port 6667) or via I2P (localhost port 6668). If you don't have an IRC client use [webchat here](http://webchat.freenode.net?channels=anoncoin).
* Ask for help on the BitcoinTalk [Anoncoin forum](https://bitcointalk.org/index.php?topic=227287.0).
