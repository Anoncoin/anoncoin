Howto build & run
==================

**Build**

Assuming you're in the root directory of the anoncoin source code.

$ `cd docker`
$ `docker -t anoncoin .`

**Run**

If you don't have a anoncoin.conf yet, the entrypoint.sh will make one for you with a random password.

$ `docker run --name anonnode -v confandblockchainOnHostPath:/home/anoncoin/.anoncoin -p 9376:9376 -p 9377:9377 -d anoncoin`

**Options**

Options are set via docker environment variables. This can be set at run with -e parameters.

* DEBUG_ENABLED        - If set to 1, debug messages is enabled (default: 0).
* CONF_MAXCONN         - Maximum remote connections (default: 500).
* CONF_TESTNET         - If set to 1, anoncoin will connect to the testnet (default: 0).
* CONF_NOTIFY_MAIL     - If a email is set, anoncoin notifies with sending a mail when a problem occurs.
* CONF_EXTRAS          - Whatever parameters you'll want. Note; only one option per line.

**Logging**

Logging happens to STDOUT as the best practise with docker containers, since infrastructure systems like kubernetes with ELK integration can automaticly forward the log to say, kibana or greylog without manual setup. :)


**Interaction with it**

Example:

$ `docker exec -ti anonnode gosu anoncoin anoncoin-cli getinfo`

On kubernetes it would be something like:

$ `kubectl exec -ti anonnode-u19231-pod81293 -- gosu anoncoin anoncoin-cli getinfo`

You can also talk to it via the json-rpc endpoint at 9376.


