#!/usr/bin/env bash

DEBUG_ENABLED=${DEBUG_ENABLED:-0}
CONF_MAXCONN=${CONF_MAXCONN:-500}
CONF_TESTNET=${CONF_TESTNET:-0}

CONF_NOTIFY_MAIL=""
if [ "$NOTIFY_MAIL" != "" ]; then
  CONF_NOTIFY_MAIL="alertnotify=echo %s | mail -s 'Anoncoin Alert' $NOTIFY_MAIL"
fi

conf_dir="/home/anoncoin/.anoncoin"
conf_file="/home/anoncoin/.anoncoin/anoncoin.conf"

# Config check
if [ ! -f "$conf_file" ]; then
  mkdir -p $conf_dir && chown anoncoin:nobody $conf_dir
  cat <<EOT >> $conf_file
rpcuser=anoncoinrpc`date +%s | sha256sum | base64 | tail -c 4 ; echo`
rpcpassword=`date +%s | sha256sum | base64 | head -c 32 ; echo`
debug=$DEBUG_ENABLED
maxconnections=$CONF_MAXCONN
testnet=$CONF_TESTNET
printtoconsole=1
daemon=0 # DO NOT SET THIS TO 1, that will break the docker instance. If anoncoind isn't in foreground, the container dies at once.
listen=1
bind=0.0.0.0
$CONF_NOTIFY_MAIL
$CONF_EXTRAS
EOT
  chown anoncoin:nobody $conf_file
fi

if [ "$1" = "anoncoind" ] || [ "$1" = "anoncoin-cli" ] || [ "$1" = "anoncoin-tx" ]; then
  exec gosu anoncoin "$1"
fi

echo
exec "$@"
