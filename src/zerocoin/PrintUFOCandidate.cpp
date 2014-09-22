// Program to display "raw" RSA UFOs (meaning no small factors have been removed).
//
// Copyright 2013-2014 The Anoncoin developers.
// Distributed under the MIT license.

using namespace std;

#include <string>
#include <iostream>
#include <fstream>
#include <exception>
#include <cstdlib>      // strtol
#include "../Zerocoin.h"



int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		uint32_t ufoIndex = strtol(argv[i], NULL, 10);
		Bignum ufo = libzerocoin::calculateRawUFO(ufoIndex, 3840);
		cout << ufoIndex << ":" << ufo.ToString(10) << endl;
	}
	return 0;
}
