FROM alpine:latest

MAINTAINER Mikal Villa <mikal@sigterm.no>

ENV GIT_BRANCH="master"
ENV ANONCOIN_PREFIX="/opt/anoncoin-${GIT_BRANCH}"
ENV PATH=${ANONCOIN_PREFIX}/bin:$PATH
ENV ANONCOIN_DATA="/home/anoncoin/.anoncoin/"

ENV GOSU_VERSION=1.7
ENV GOSU_SHASUM="34049cfc713e8b74b90d6de49690fa601dc040021980812b2f1f691534be8a50  /usr/local/bin/gosu"

ENV BERKELEYDB_VERSION=db-4.8.30.NC
ENV BERKELEYDB_PREFIX=/opt/${BERKELEYDB_VERSION}

RUN apk --no-cache --virtual build-dependendencies add autoconf automake make gcc g++ libtool pkgconfig git boost-dev build-base openssl-dev git

RUN adduser -S anoncoin

RUN mkdir -p /tmp/build && cd /tmp/build && git clone -b ${GIT_BRANCH} https://github.com/Anoncoin/anoncoin.git \
    && wget -O /tmp/build/${BERKELEYDB_VERSION}.tar.gz http://download.oracle.com/berkeley-db/${BERKELEYDB_VERSION}.tar.gz \
    && tar -xzf /tmp/build/${BERKELEYDB_VERSION}.tar.gz -C /tmp/build/ \
    && sed s/__atomic_compare_exchange/__atomic_compare_exchange_db/g -i /tmp/build/${BERKELEYDB_VERSION}/dbinc/atomic.h \
    && mkdir -p ${BERKELEYDB_PREFIX} \
    && cd /tmp/build/${BERKELEYDB_VERSION}/build_unix \
    && ../dist/configure --enable-cxx --disable-shared --with-pic --prefix=${BERKELEYDB_PREFIX} \
    && make install

RUN apk --no-cache add openssl \
    && wget -O /usr/local/bin/gosu https://github.com/tianon/gosu/releases/download/${GOSU_VERSION}/gosu-amd64 \
    && echo "${GOSU_SHASUM}" | sha256sum -c \
    && chmod +x /usr/local/bin/gosu

RUN cd /tmp/build/anoncoin && ./autogen.sh && ./configure LDFLAGS=-L${BERKELEYDB_PREFIX}/lib \
    CPPFLAGS=-I${BERKELEYDB_PREFIX}/include --prefix=${ANONCOIN_PREFIX} --without-qrencode \
    --without-miniupnpc --with-gui=no --disable-shared --enable-hardening --disable-tests \
    --with-pic --disable-ccache --with-daemon --with-utils && make -j4 && make install

#RUN strip ${ANONCOIN_PREFIX}/bin/anoncoind && ${ANONCOIN_PREFIX}/bin/anoncoin-tx ${ANONCOIN_PREFIX}/bin/anoncoin-cli \
RUN rm -rf /tmp/build ${BERKELEYDB_PREFIX}/docs && apk --no-cache --purge del \
    autoconf automake make gcc g++ libtool pkgconfig git boost-dev build-base openssl-dev git \
    && apk --no-cache add boost boost-program_options

COPY entrypoint.sh /entrypoint.sh
RUN chmod a+x /entrypoint.sh

VOLUME [ "/home/anoncoin/.anoncoin" ]

EXPOSE 19377 9377 9376 19376

ENTRYPOINT [ "/entrypoint.sh" ]

CMD [ "anoncoind" ]
