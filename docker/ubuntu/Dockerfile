FROM ubuntu:16.04

MAINTAINER Mikal Villa <mikal@sigterm.no>

RUN apt update && apt install -y build-essential git vim libboost-all-dev pkg-config automake autoconf libtool libdb++-dev libssl-dev

RUN cd /usr/src && git clone https://github.com/Anoncoin/anoncoin.git

RUN cd /usr/src/anoncoin && ./autogen.sh && ./configure --prefix=/usr/local --without-qrencode --without-miniupnpc --with-gui=no \
                                --disable-shared --enable-hardening --disable-tests --with-pic --with-incompatible-bdb

RUN cd /usr/src/anoncoin && make -j4 && make install

RUN apt remove -y automake autoconf git libtool build-essential && apt autoremove -y && apt-get clean && rm -rf /var/lib/apt/lists/*

VOLUME [ "/root/.anoncoin" ]

COPY entrypoint.sh /entrypoint.sh

RUN chmod a+x /entrypoint.sh

EXPOSE 19377 9377 9376 19376

ENTRYPOINT [ "/entrypoint.sh" ]
