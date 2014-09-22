# Introduction

The code in this directory (which was formerly known as libzerocoin) implements the core cryptographic routines of the Zerocoin protocol. Zerocoin is a distributed anonymous cash extension for Bitcoin-type (hash chain based) protocols. The protocol uses zero knowledge proofs to implement a fully decentralized coin laundry.

The Zerocoin protocol is provably secure and uses well-studied cryptographic primitives. For a complete description of the protocol, see the Zerocoin team's white paper published in the IEEE Security & Privacy Symposium (2013) below.

### WARNING
As of 2014-09-21, this code has not been thoroughly tested for vulnerabilities that could lead to crashes, arbitrary code execution on your computer, or loss of your coins or anonymity. Use for testing only!

### Warning to Anoncoin developers
Many classes here store a pointer to an instance of Params, so it should be a global variable or otherwise present in memory whenever there is a possibility of Zerocoin code being called. Failure to ensure this will lead to memory corruption.


# Overview

This code implements the core cryptographic operations of Zerocoin. These include:

1. Parameter generation
2. Coin generation ("Minting")
3. Coin spending (generation of a zero knowledge proof)
4. Accumulator calculation
5. Coin and spend proof verification

This code does _not_ implement the full Zerocoin protocol. In addition to the above cryptographic routines, a full Zerocoin implementation requires several specialized Zerocoin messages, double spending checks, and some additional coin redemption logic that must be supported by all clients in the network. This code does not provide routines to support these functions, although we do provide an overview on the [[Integrating with Bitcoin clients]] page.

# Outside links

* [Zerocoin Project website](http://zerocoin.org/)
* [Zerocoin Paper](http://zerocoin.org/media/pdf/ZerocoinOakland.pdf)
