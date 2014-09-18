Anoncoin
====================

Anoncoin is a privacy-oriented decentralized crypto-currency which uses the I2P network to hide users' identities (Tor is also supported).

Platforms:
- Linux - [![Build Status - Linux](https://jenkins.nordcloud.no/buildStatus/icon?job=Anoncoin-Linux)](https://jenkins.nordcloud.no/job/Anoncoin-Linux/)
- Windows (Mingw-w64) - [![Build Status - Mingw-w64](https://jenkins.nordcloud.no/buildStatus/icon?job=Anoncoin-Mingw-w64)](https://jenkins.nordcloud.no/job/Anoncoin-Mingw-w64/)
- OSX (Build status to be added soon)

Specs:
 - launched in June 2013
 - scrypt proof-of-work (same as Litecoin)
 - 3 minute block targets with KGW retargeting (before block 87777, it was 3.42 minute targets using classic algorithm)
 - Starts with 4.2 coin blocks until block 42000
 - 7 coin blocks until block 77777
 - 5 coin blocks after 77778, then subsidy halves when block height divisible by 306600 (~1.75 years)
 - total supply will be 3,103,954 ANC

Development process
===================

Developers work in their own trees, then submit pull requests when
they think their feature or bug fix is ready.

The patch will be accepted if there is broad consensus that it is a
good thing.  Developers should expect to rework and resubmit patches
if they don't match the project's coding conventions (see coding.txt)
or are controversial.

The master branch is regularly built and tested, but is not guaranteed
to be completely stable. Tags are regularly created to indicate new
official, stable release versions of Anoncoin.

Feature branches are created when there are major new features being
worked on by several people.

From time to time a pull request will become outdated. If this occurs, and
the pull is no longer automatically mergeable; a comment on the pull will
be used to issue a warning of closure. The pull will be closed 15 days
after the warning if action is not taken by the author. Pull requests closed
in this manner will have their corresponding issue labeled 'stagnant'.

Issues with no commits will be given a similar warning, and closed after
15 days from their last activity. Issues closed in this manner will be
labeled 'stale'.

