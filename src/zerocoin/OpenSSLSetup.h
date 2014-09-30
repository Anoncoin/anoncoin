#ifndef OPENSSL_SETUP_H_
#define OPENSSL_SETUP_H_

#include <openssl/crypto.h>
#include <boost/thread/recursive_mutex.hpp>

// this should be present if and only if Zerocoin is used outside of anoncoin{d,-qt},
// since otherwise the proper setup is already being performed (see util.cpp).

static boost::recursive_mutex** ppmutexOpenSSL;

void locking_callback(int mode, int i, const char* file, int line)
{   
    if (mode & CRYPTO_LOCK) {
        ppmutexOpenSSL[i]->lock();
    } else {
        ppmutexOpenSSL[i]->unlock();
    }
}

void SetupPRNGThreading()
{
	// Init OpenSSL library multithreading support
	ppmutexOpenSSL = (boost::recursive_mutex**)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(boost::recursive_mutex*)); 
	for (int i = 0; i < CRYPTO_num_locks(); i++)
		ppmutexOpenSSL[i] = new boost::recursive_mutex();
	CRYPTO_set_locking_callback(locking_callback);
}

void TearDownPRNGThreading()
{
	// Shutdown OpenSSL library multithreading support
	CRYPTO_set_locking_callback(NULL);
	for (int i = 0; i < CRYPTO_num_locks(); i++)
		delete ppmutexOpenSSL[i];
	OPENSSL_free(ppmutexOpenSSL);
}



#endif /* ifndef OPENSSL_SETUP_H_ */
