#!/usr/bin/env python2.7
# Copyright (c) 2014 The Anoncoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from Crypto.Hash import SHA256


def print_ufo_hashes(start, end):
    """Generates the preprocessor definition of a UFO precursor ("precursor"
    meaning prior to dividing out small factors)."""

    assert type(start) in [int, long] and start >= 1
    assert type(end) in [int, long] and end >= start

    print "#define UFO_PRECURSOR_%d_TO_%d \\" % (start, end)

    for i in xrange(start, end+1):
        ending = ' \\'
        if i == end:
            ending = ''
        print '    "%s"%s' % (SHA256.new(str(i)).hexdigest(), ending)
    
    print ""


print \
"""// Auto-generated file. Do not edit; your changes will be lost!
#ifndef ANONCOIN_ZC_UFO_PRE_GEN_H
#define ANONCOIN_ZC_UFO_PRE_GEN_H
"""

print_ufo_hashes( 1, 40)
print_ufo_hashes(41, 80)

print "#endif /* ifndef ANONCOIN_ZC_UFO_PRE_GEN_H */"
