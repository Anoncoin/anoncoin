The Anoncoin project
====================

Build Statuses
---------------
- Linux build - [![Build Status - Linux](https://jenkins.nordcloud.no/buildStatus/icon?job=Anoncoin-Linux)](https://jenkins.nordcloud.no/job/Anoncoin-Linux/)
- Mingw-w64 (Windows) - [![Build Status - Mingw-w64](https://jenkins.nordcloud.no/buildStatus/icon?job=Anoncoin-Mingw-w64)](https://jenkins.nordcloud.no/job/Anoncoin-Mingw-w64/)
- OSX (Too be added real soon)


Specs:

Anoncoin - a fork version of Litecoin optimized for CPU mining using scrypt as a proof of work scheme.
 - 3.42 minute block targets (retarget on ~1680 blocks, 4days)
 - Starts with 4.2 coin blocks until block 42000
 - 7 coin blocks until block 77777
 - 5 coin blocks after 77778, then subsidy halves in 306600 blocks (~2 years)

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

