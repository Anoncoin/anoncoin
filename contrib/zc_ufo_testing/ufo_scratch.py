#!/usr/bin/python

# I hacked together this code to try to estimate the probability that a
# 3800-bit RSA UFO is secure. See the categorize() function below. The idea is
# to "scale down" the problem to UFO sizes that I can factorize with msieve.
#
# My assumption: a 3800-bit RSA UFO is secure if, after removing all prime
# factors less than 500 bits, the result is composite and greater than 2000
# bits. If we divide these two parameters by 3800, we get two bit fractions
# that can be applied to smaller (readily factorizable) numbers. I introduce
# an additional parameter for the largest prime factor that myself and the
# other Anoncoin team members can readily divide out from the candidate UFOs.
# 180 bits sounds reasonable for this parameter.
#
# Fortunately, it appears that the probability (defined in terms of bit
# fractions) is equal for any bit length; for the parameters above, it is about
# 0.79.

from math import log
from Crypto.Hash import SHA256
from threading import Thread
from subprocess import Popen, PIPE
from collections import defaultdict

lg = lambda x: log(x,2)

def H(n, bits=232):
    assert bits % 4 == 0
    s = ''
    i = 0
    while len(s) < (bits/4):
      h = SHA256.new()
      if i == 0:
          b = str(n)    # backwards compat
      else:
          b = str(n) + '||' + str(i)
      h.update(b)
      s += h.hexdigest()
      i += 1
    s = s[:(bits/4)]
    return long(s, 16)

# I do this in a separate thread so I can use a REPL while the factoring is in
# progress.
def run():
    global factor_arr
    i = len(factor_arr)+1
    while True:
        n = H(i, bits=288)   # 100 144 188 232 256 276 288
        stdout = Popen(['msieve','-q', '-e', str(n).rstrip('L')], stdin=PIPE, stdout=PIPE).communicate('')[0]
        p = parse_output(stdout)
        factor_arr.append(p)
        if len(factor_arr) >= 500: break  #XXX
        i += 1

t = Thread(target=run); t.daemon = True; t.start()

def categorize(tup, easy_ecm_frac=(180./3800), ecm_frac=(500./3800), gnfs_frac=(2000./3800)):
    """
    easy_ecm_frac: bit fraction of largest factor we can remove by ECM factorization.
    ecm_frac: bit fraction of largest factor an attacker could remove by ECM.
    gnfs_frac: bit fraction of largest composite an attacker could factorize by GNFS.
    """
    n = tup[0]
    f = tup[1][:]
    assert len(f) > 0
    if len(f) == 1:
        return 'can_factor'
    lg_n = lg(n)
    easy_ecm_bits = easy_ecm_frac*lg_n
    ecm_bits = ecm_frac*lg_n
    gnfs_bits = gnfs_frac*lg_n
    f = filter(lambda x: lg(x) > easy_ecm_bits, f)
    if len(f) < 2:
        return 'can_factor'
    if lg(reduce(lambda x,y:x*y, f)) < gnfs_bits:
        return 'can_reject'
    f = filter(lambda x: lg(x) > ecm_bits, f)
    if len(f) < 2:
        return 'invalid_ecm'
    x = reduce(lambda x,y:x*y, f)
    if lg(x) < gnfs_bits:
        return 'invalid_gnfs'
    f.sort()
    if lg(f[-2]) < gnfs_bits/2:
        return 'valid_lopsided'
    return 'valid'


# estimated probability that a UFO of this bit size is secure
P = lambda c: float(c['valid']+c['valid_lopsided'])/(sum(c.values())-c['can_factor'])

P(reduce(f, map(categorize, factor_arr[:]), defaultdict(int)))
