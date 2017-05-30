// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"

#include "clientversion.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "protocol.h"
#include "sync.h"
#include "util.h"
#include "alert.h"
#include "base58.h"
#include "addrman.h"

#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()

#include "json/json_spirit_value.h"

using namespace json_spirit;
using namespace std;

Value getconnectioncount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getconnectioncount\n"
            "\n Returns the number of connections to other nodes.\n"
            "\nbResult:\n"
            "n          (numeric) The connection count\n"
            "\nExamples:\n"
            + HelpExampleCli("getconnectioncount", "")
            + HelpExampleRpc("getconnectioncount", "")
        );

    LOCK2(cs_main, cs_vNodes);
    return (int)vNodes.size();
}

Value ping(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "ping\n"
            "\n Requests that a ping be sent to all other nodes, to measure ping time.\n"
            " Results provided in getpeerinfo, pingtime and pingwait fields are decimal seconds.\n"
            " Ping command is handled in queue with all other commands, so it measures processing backlog, not just network ping.\n"
            "\nExamples:\n"
            + HelpExampleCli("ping", "")
            + HelpExampleRpc("ping", "")
        );

    // Request that each node send a ping during next message processing pass
    LOCK2(cs_main, cs_vNodes);
    BOOST_FOREACH(CNode* pNode, vNodes) {
        pNode->fPingQueued = true;
    }

    return Value::null;
}

Value destination(const Array& params, bool fHelp)
{
   if (fHelp || params.size() > 2)
        throw runtime_error(
            "destination [\"match|good|attempt|connect\"] [\"b32.i2p|base64|ip:port\"]\n"
            "\n Returns I2P destination details stored in your b32.i2p address manager lookup system.\n"
            "\nArguments:\n"
            "  If no arguments are provided, the command returns all the b32.i2p addresses. NOTE: Results will not include base64\n"
            "  1st argument = \"match\" then a 2nd argument is also required.\n"
            "  2nd argument = Any string. If a match is found in any of the address, source or base64 fields, that result will be returned.\n"
            "  1st argument = \"good\" destinations that has been tried, connected and found to be good will be returned.\n"
            "  1st argument = \"attempt\" destinations that have been attempted, will be returned.\n"
            "  1st argument = \"connect\" destinations that have been connected to in the past, will be returned.\n"
            "\nResults are returned as a json array of object(s).\n"
            "  The 1st result pair is the total size of the address hash map.\n"
            "  The 2nd result pair is the number of objects which follow, as matching this query.  It can be zero, if no match was found.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"tablesize\": nnn,             (numeric) The total number of destinations in the i2p address book\n"
            "    \"matchsize\": nnn,             (numeric) The number of results returned, which matched your query\n"
            "  }\n"
            "  {\n"
            "    \"address\":\"b32.i2p\",          (string)  Base32 hash of a i2p destination, a possible peer\n"
            "    \"good\": true|false,           (boolean) Has this address been tried & found to be good\n"
            "    \"attempt\": nnn,               (numeric) The number of times it has been attempted\n"
            "    \"lasttry\": ttt,               (numeric) The time of a last attempted connection (memory only)\n"
            "    \"connect\": ttt,               (numeric) The time of a last successful connection\n"
            "    \"source\":\"b32.i2p|ip:port\",   (string)  The source of information about this address\n"
            "    \"base64\":\"destination\",       (string)  The full Base64 Public Key of this peers b32.i2p address\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nNOTE: The results obtained are only a snapshot, while you are connected to the network.\n"
            "      Peers are updating addresses & destinations all the time.\n"
            "\nExamples: Return all I2P destinations currently known about on the system.\n"
            + HelpExampleCli("destination", "")
            + HelpExampleRpc("destination", "") +
            "\nExamples: Return the I2P destinations marked as 'good', happens if they have been tried and a successful version handshake made.\n"
            + HelpExampleCli("destination", "good") +
            "\nExample: Return I2P destinations marked as having made an attempt to connect\n"
            + HelpExampleRpc("destination", "attempt") +
            "\nExample: Return I2P destinations which are marked as having been connected to.\n"
            + HelpExampleCli("destination", "connect") +
            "\nExamples: Return I2P destination entries which came from the 'source' IP address 215.49.103.xxx\n"
            + HelpExampleRpc("destination", "match 215.49.103") +
            "\nExamples: Return all I2P b32.i2p destinations which match the patter, these could be found in the 'source' or the 'address' fields.\n"
            + HelpExampleCli("destination", "match vatzduwjheyou3ybknfgm7cl43efbhovtrpfduz55uilxahxwt7a.b32.i2p")
            + HelpExampleRpc("destination", "match vatzduwjheyou3ybknfgm7cl43efbhovtrpfduz55uilxahxwt7a.b32.i2p")
        );
    //! We must not have node or main processing as Addrman needs to
    //! be considered static for the time required to process this.
    LOCK2(cs_main, cs_vNodes);

    bool fSelectedMatch = false;
    bool fMatchStr = false;
    bool fMatchTried = false;
    bool fMatchAttempt = false;
    bool fMatchConnect = false;
    bool fUnknownCmd = false;
    string sMatchStr;

    if( params.size() > 0 ) {                                   // Lookup the address and return the one object if found
        string sCmdStr = params[0].get_str();
        if( sCmdStr == "match" ) {
            if( params.size() > 1 ) {
                sMatchStr = params[1].get_str();
                fMatchStr = true;
            } else
                fUnknownCmd = true;
        } else if( sCmdStr == "good" )
            fMatchTried = true;
        else if( sCmdStr == "attempt" )
            fMatchAttempt = true;
        else if( sCmdStr == "connect")
            fMatchConnect = true;
        else
            fUnknownCmd = true;
        fSelectedMatch = true;
    }

    Array ret;
    // Load the vector with all the objects we have and return with
    // the total number of addresses we have on file
    vector<CDestinationStats> vecStats;
    int nTableSize = addrman.CopyDestinationStats(vecStats);
    if( !fUnknownCmd ) {       // If set, throw runtime error
        for( int i = 0; i < 2; i++ ) {      // Loop through the data twice
            bool fMatchFound = false;       // Assume no match
            int nMatchSize = 0;             // the match counter
            BOOST_FOREACH(const CDestinationStats& stats, vecStats) {
                if( fSelectedMatch ) {
                    if( fMatchStr ) {
                        if( stats.sAddress.find(sMatchStr) != string::npos ||
                            stats.sSource.find(sMatchStr) != string::npos ||
                            stats.sBase64.find(sMatchStr) != string::npos )
                                fMatchFound = true;
                    } else if( fMatchTried ) {
                        if( stats.fInTried ) fMatchFound = true;
                    }
                    else if( fMatchAttempt ) {
                        if( stats.nAttempts > 0 ) fMatchFound = true;
                    }
                    else if( fMatchConnect ) {
                        if( stats.nSuccessTime > 0 ) fMatchFound = true;
                    }
                } else          // Match everything
                    fMatchFound = true;

                if( i == 1 && fMatchFound ) {
                    Object obj;
                    obj.push_back(Pair("address", stats.sAddress));
                    obj.push_back(Pair("good", stats.fInTried));
                    obj.push_back(Pair("attempt", stats.nAttempts));
                    obj.push_back(Pair("lasttry", stats.nLastTry));
                    obj.push_back(Pair("connect", stats.nSuccessTime));
                    obj.push_back(Pair("source", stats.sSource));
                    //! Do to an RPC buffer limit of 65535 with stream output, we can not send these and ever get a result
                    //! This should be considered a short term fix ToDo:  Allocate bigger iostream buffer for the output
                    if( fSelectedMatch )
                        obj.push_back(Pair("base64", stats.sBase64));
                    ret.push_back(obj);
                }
                if( fMatchFound ) {
                    nMatchSize++;
                    fMatchFound = false;
                }
            }
            // The 1st time we get a count of the matches, so we can list that first in the results,
            // then we finally build the output objects, on the 2nd pass...and don't put this in there twice
            if( i == 0 ) {
                Object objSizes;
                objSizes.push_back(Pair("tablesize", nTableSize));
                objSizes.push_back(Pair("matchsize", nMatchSize));
                ret.push_back(objSizes);                            // This is the 1st object put on the Array
            }
        }
    } else
        throw runtime_error( "Unknown subcommand or argument missing" );

    return ret;
}

static void CopyNodeStats(std::vector<CNodeStats>& vstats)
{
    vstats.clear();

    LOCK(cs_vNodes);
    vstats.reserve(vNodes.size());
    BOOST_FOREACH(CNode* pnode, vNodes) {
        CNodeStats stats;
        pnode->copyStats(stats);
        vstats.push_back(stats);
    }
}


Value getpeerinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpeerinfo\n"
            "\n Returns data about each connected network node as a json array of objects.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"id\": n,                       (numeric) Peer index\n"
            "    \"addr\":\"host:port|b32.i2p\",    (string) The ip:port address or i2p b32.i2p destination of the peer\n"
            "    \"addrlocal\":\"ip:port|b32.i2p\", (string) Our local address as known by the peer\n"
            "    \"services\":\"xxxxxxxxxxxxxxxx\", (string) The services offered, all 64 bits in hex\n"
            "    \"lastsend\": ttt,               (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last send\n"
            "    \"lastrecv\": ttt,               (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last receive\n"
            "    \"bytessent\": n,                (numeric) The total bytes sent\n"
            "    \"bytesrecv\": n,                (numeric) The total bytes received\n"
            "    \"conntime\": ttt,               (numeric) The connection time in seconds since epoch (Jan 1 1970 GMT)\n"
            "    \"pingtime\": n,                 (numeric) ping time\n"
            "    \"pingwait\": n,                 (numeric) ping wait\n"
            "    \"version\": v,                  (numeric) The peer protocol version, such as 70009\n"
            "    \"subver\": \"/s:n.n.n.n/\",       (string) The subversion string such as Anonymous:0.9.5\n"
            "    \"inbound\": true|false,         (boolean) Inbound (true) or Outbound (false)\n"
            "    \"startingheight\": n,           (numeric) The starting height (block) of the peer\n"
            "    \"banscore\": n,                 (numeric) The ban score\n"
            "    \"synced_headers\": n,           (numeric) The last header we have in common with this peer\n"
            "    \"synced_blocks\": n,            (numeric) The last block we have in common with this peer\n"
            "    \"inflight\": [\n"
            "       n,                          (numeric) The heights of blocks we're currently asking from this peer\n"
            "       ...\n"
            "    ]\n"
            "    \"whitelisted\": true|false,     (boolean) This peer is considered whitelisted (true) or not (false)\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("getpeerinfo", "")
            + HelpExampleRpc("getpeerinfo", "")
        );

    LOCK(cs_main);

    vector<CNodeStats> vstats;
    CopyNodeStats(vstats);

    Array ret;

    BOOST_FOREACH(const CNodeStats& stats, vstats) {
        Object obj;
        CNodeStateStats statestats;
        bool fStateStats = GetNodeStateStats(stats.nodeid, statestats);
        obj.push_back(Pair("id", stats.nodeid));
        obj.push_back(Pair("addr", stats.addrName));
        if (!(stats.addrLocal.empty()))
            obj.push_back(Pair("addrlocal", stats.addrLocal));
        obj.push_back(Pair("services", strprintf("%016x", stats.nServices)));
        obj.push_back(Pair("lastsend", stats.nLastSend));
        obj.push_back(Pair("lastrecv", stats.nLastRecv));
        obj.push_back(Pair("bytessent", stats.nSendBytes));
        obj.push_back(Pair("bytesrecv", stats.nRecvBytes));
        obj.push_back(Pair("conntime", stats.nTimeConnected));
        obj.push_back(Pair("pingtime", stats.dPingTime));
        if (stats.dPingWait > 0.0)
            obj.push_back(Pair("pingwait", stats.dPingWait));
        obj.push_back(Pair("version", stats.nVersion));
        // Use the sanitized form of subver here, to avoid tricksy remote peers from
        // corrupting or modifiying the JSON output by putting special characters in
        // their ver message.
        obj.push_back(Pair("subver", stats.cleanSubVer));
        obj.push_back(Pair("inbound", stats.fInbound));
        obj.push_back(Pair("startingheight", stats.nStartingHeight));
        if (fStateStats) {
            obj.push_back(Pair("banscore", statestats.nMisbehavior));
            obj.push_back(Pair("synced_headers", statestats.nSyncHeight));
            obj.push_back(Pair("synced_blocks", statestats.nCommonHeight));
            Array heights;
            BOOST_FOREACH(int height, statestats.vHeightInFlight) {
                heights.push_back(height);
            }
            obj.push_back(Pair("inflight", heights));
        }
        obj.push_back(Pair("whitelisted", stats.fWhitelisted));

        ret.push_back(obj);
    }

    return ret;
}

Value addnode(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() == 2)
        strCommand = params[1].get_str();
    if (fHelp || params.size() != 2 ||
        (strCommand != "onetry" && strCommand != "add" && strCommand != "remove"))
        throw runtime_error(
            "addnode <\"b32.i2p|base64|ip:port|ipv6\"> <\"add|remove|onetry\">\n"
            "\n Attempts add or remove a node from the addnode list.\n"
            " Or try a connection to a node once.\n"
            "\nArguments:\n"
            " 1. (string, required) Any valid IP Address, can be IPv4,IPv6 and include an option port or subnet range.  I2P Destinations can be Base64 or b32.i2p format.\n"
            " 2. (string, required) 'add' to add a node to the list, 'remove' to remove a node from the list, 'onetry' to try a connection to the specified location once.\n"
            "\nExamples:\n"
            + HelpExampleCli("addnode", "192.168.0.6:9377 onetry")
            + HelpExampleRpc("addnode", "192.168.0.6:9377 add")
            + HelpExampleCli("addnode", "ibtfn3cnherivbkfbytay5tx35saajauxlg2aohna2rwyci2pecq.b32.i2p remove")
            + HelpExampleRpc("addnode", "ibtfn3cnherivbkfbytay5tx35saajauxlg2aohna2rwyci2pecq.b32.i2p onetry")
        );

    string strNode = params[0].get_str();

    if (strCommand == "onetry")
    {
        CAddress addr;
        // The Oneshot flag 'could' be true here...
        // If it is true, then the node if connects, will be queried for addresses
        // and disconnected, so for this command we call it set false, as we want
        // stay connected if we can.
        OpenNetworkConnection(addr, NULL, strNode.c_str(), false);
        return Value::null;
    }

    LOCK(cs_vAddedNodes);
    vector<string>::iterator it = vAddedNodes.begin();
    for(; it != vAddedNodes.end(); it++)
        if (strNode == *it)
            break;

    if (strCommand == "add")
    {
        if (it != vAddedNodes.end())
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: Node already added");
        vAddedNodes.push_back(strNode);
    }
    else if(strCommand == "remove")
    {
        if (it == vAddedNodes.end())
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
        vAddedNodes.erase(it);
    }

    return Value::null;
}

Value getaddednodeinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getaddednodeinfo <dns> [\"node\"]\n"
            "\n Returns information about the given added node, or all added nodes\n"
            " (note that onetry addnodes are not listed here)\n"
            " If dns is false, only a list of added nodes will be provided,\n"
            " otherwise connected information will also be available.\n"
            "\nArguments:\n"
            " 1. dns      (boolean, required) If false, only a list of added nodes will be provided, otherwise connected information will also be available.\n"
            " 2. \"node\"   (string, optional) If provided, return information about this specific node, otherwise all nodes are returned.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"addednode\" : \"192.168.0.201\",         (string) The node ip address\n"
            "    \"connected\" : true|false,              (boolean) If connected\n"
            "    \"addresses\" : [\n"
            "       {\n"
            "         \"address\" : \"192.168.0.201:9377\", (string) The anoncoin server host and port\n"
            "         \"connected\" : \"outbound\"          (string) connection, inbound or outbound\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddednodeinfo", "true")
            + HelpExampleCli("getaddednodeinfo", "true \"192.168.0.201\"")
            + HelpExampleRpc("getaddednodeinfo", "true, \"192.168.0.201\"")
        );

    bool fDns = params[0].get_bool();

    list<string> laddedNodes(0);
    if (params.size() == 1)
    {
        LOCK(cs_vAddedNodes);
        BOOST_FOREACH(string& strAddNode, vAddedNodes)
            laddedNodes.push_back(strAddNode);
    }
    else
    {
        string strNode = params[1].get_str();
        LOCK(cs_vAddedNodes);
        BOOST_FOREACH(string& strAddNode, vAddedNodes)
            if (strAddNode == strNode)
            {
                laddedNodes.push_back(strAddNode);
                break;
            }
        if (laddedNodes.size() == 0)
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
    }

    Array ret;
    if (!fDns)
    {
        BOOST_FOREACH(string& strAddNode, laddedNodes)
        {
            Object obj;
            obj.push_back(Pair("addednode", strAddNode));
            ret.push_back(obj);
        }
        return ret;
    }

    list<pair<string, vector<CService> > > laddedAddreses(0);
    BOOST_FOREACH(string& strAddNode, laddedNodes)
    {
        vector<CService> vservNode(0);
        if(Lookup(strAddNode.c_str(), vservNode, isStringI2pDestination(strAddNode) ? 0 : Params().GetDefaultPort(), fNameLookup, 0))
            laddedAddreses.push_back(make_pair(strAddNode, vservNode));
        else
        {
            Object obj;
            obj.push_back(Pair("addednode", strAddNode));
            obj.push_back(Pair("connected", false));
            Array addresses;
            obj.push_back(Pair("addresses", addresses));
        }
    }

    LOCK(cs_vNodes);
    for (list<pair<string, vector<CService> > >::iterator it = laddedAddreses.begin(); it != laddedAddreses.end(); it++)
    {
        Object obj;
        obj.push_back(Pair("addednode", it->first));

        Array addresses;
        bool fConnected = false;
        BOOST_FOREACH(CService& addrNode, it->second)
        {
            bool fFound = false;
            Object node;
            node.push_back(Pair("address", addrNode.ToString()));
            BOOST_FOREACH(CNode* pnode, vNodes)
                if (pnode->addr == addrNode)
                {
                    fFound = true;
                    fConnected = true;
                    node.push_back(Pair("connected", pnode->fInbound ? "inbound" : "outbound"));
                    break;
                }
            if (!fFound)
                node.push_back(Pair("connected", "false"));
            addresses.push_back(node);
        }
        obj.push_back(Pair("connected", fConnected));
        obj.push_back(Pair("addresses", addresses));
        ret.push_back(obj);
    }

    return ret;
}

Value getnettotals(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "getnettotals\n"
            "\n Returns information about network traffic, including bytes in, bytes out, and current time.\n"
            "\nResult:\n"
            "{\n"
            "  \"totalbytesrecv\": n,   (numeric) Total bytes received by this Node.\n"
            "  \"totalbytessent\": n,   (numeric) Total bytes sent by this Node.\n"
            "  \"timemillis\": t        (numeric) The current Unix timestamp in milliSeconds.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getnettotals", "")
            + HelpExampleRpc("getnettotals", "")
       );

    Object obj;
    obj.push_back(Pair("totalbytesrecv", CNode::GetTotalBytesRecv()));
    obj.push_back(Pair("totalbytessent", CNode::GetTotalBytesSent()));
    obj.push_back(Pair("timemillis", GetTimeMillis()));
    return obj;
}

static Array GetNetworksInfo()
{
    Array networks;
    for(int n=0; n<NET_MAX; ++n)
    {
        enum Network network = static_cast<enum Network>(n);
        if(network == NET_UNROUTABLE)
            continue;
        proxyType proxy;
        Object obj;
        GetProxy(network, proxy);
        obj.push_back(Pair("name", GetNetworkName(network)));
        obj.push_back(Pair("limited", IsLimited(network)));
        obj.push_back(Pair("reachable", IsReachable(network)));
        obj.push_back(Pair("proxy", proxy.IsValid() ? proxy.ToStringIPPort() : string()));
        networks.push_back(obj);
    }
    return networks;
}

static const string SingleAlertSubVersionsString( const std::set<std::string>& setVersions )
{
    std::string strSetSubVer;
    BOOST_FOREACH(std::string str, setVersions) {
        if(strSetSubVer.size())                 // Must be more than one
            strSetSubVer += " or ";
        strSetSubVer += str;
    }
    return strSetSubVer;
}

Value getnetworkinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getnetworkinfo\n"
            " Returns an object containing various state info regarding P2P networking.\n"
            "\nResult:\n"
            "{\n"
            "  \"version\": xxxxx,            (numeric) the server version\n"
            "  \"subver\": \"/s:n.n.n.n/\",     (string)  this clients subversion string\n"
            "  \"protocolversion\": xxxxx,    (numeric) the protocol version\n"
            "  \"localservices\": \"xxxx\",     (hex string) Our local service bits as a 16 char string.\n"
            "  \"timeoffset\": xxxxx,         (numeric) the time offset\n"
            "  \"connections\": xxxxx,        (numeric) the number of connections\n"
            "  \"relayfee\": x.xxxx,          (numeric) minimum relay fee for non-free transactions in anc/kb\n"
            "  \"networkconnections\": [      (array)  the state of each possible network connection type\n"
            "    \"name\": \"xxx\",             (string) network name\n"
            "    \"limited\" : true|false,    (boolean) if service is limited\n"
            "    \"reachable\" : true|false,  (boolean) if service is reachable\n"
            "    \"proxy\": \"host:port\"       (string, optional) the proxy used by the server\n"
            "  ]\n"
            "  \"localaddresses\": [          (array) list of local addresses\n"
            "    \"address\": \"xxxx\",         (string) network address\n"
            "    \"port\": xxx,               (numeric) network port\n"
            "    \"score\": xxx               (numeric) relative score\n"
            "  ]\n"
            "  \"alerts\": [                  (array) list of alerts on network\n"
            "    \"alertid\": \"xxx\",          (numeric) the ID number for this alert\n"
            "    \"priority\": xxx,           (numeric) the alert priority\n"
            "    \"minver\": xxx,             (numeric) the minimum protocol version this effects\n"
            "    \"maxver\": xxx,             (numeric) the maximum protocol version this effects\n"
            "    \"subvers\": \"/s:n.n.n.n/\",  (string) null=all or the client version(s) this effects\n"
            "    \"relayuntil\": xxx,         (numeric) relay this alert to other nodes until this time\n"
            "    \"expiration\": xxx,         (numeric) when this alert will expire\n"
            "    \"statusbar\": \"xxxx\"        (string) status bar & tooltip string displayed\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getnetworkinfo", "")
            + HelpExampleRpc("getnetworkinfo", "")
        );

    LOCK(cs_main);

    Object obj;
    obj.push_back(Pair("version",        (int)CLIENT_VERSION));
    obj.push_back(Pair("subversion",     FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<string>())));
    obj.push_back(Pair("protocolversion",(int)PROTOCOL_VERSION));
    obj.push_back(Pair("localservices",  strprintf("%016x", nLocalServices)));
    obj.push_back(Pair("timeoffset",     GetTimeOffset()));
    obj.push_back(Pair("connections",    (int)vNodes.size()));
    obj.push_back(Pair("relayfee",       ValueFromAmount(minRelayTxFee.GetFeePerK())));
    obj.push_back(Pair("networkconnections",GetNetworksInfo()));
    Array localAddresses;
    {
        LOCK(cs_mapLocalHost);
        BOOST_FOREACH(const PAIRTYPE(CNetAddr, LocalServiceInfo) &item, mapLocalHost)
        {
            Object rec;
            rec.push_back(Pair("address", item.first.ToString()));
            rec.push_back(Pair("port", item.second.nPort));
            rec.push_back(Pair("score", item.second.nScore));
            localAddresses.push_back(rec);
        }
    }
    obj.push_back(Pair("localaddresses", localAddresses));

    // Add in the list of alerts currently on the network
    Array localAlerts;
    if( !mapAlerts.empty() ) {
          // Parse all the alerts, and prepare a JSON response list
          LOCK(cs_mapAlerts);
          for( map<uint256, CAlert>::iterator mi = mapAlerts.begin(); mi != mapAlerts.end(); mi++ ) {
               const CAlert& alert = (*mi).second;
               Object rec;
               rec.push_back( Pair("alertid", alert.nID) );
               rec.push_back( Pair("priority", alert.nPriority) );
               rec.push_back( Pair("minver", alert.nMinVer) );
               rec.push_back( Pair("maxver", alert.nMaxVer) );
               rec.push_back( Pair("subver", SingleAlertSubVersionsString(alert.setSubVer)) );
               rec.push_back( Pair("relayuntil", alert.nRelayUntil) );
               rec.push_back( Pair("expiration", alert.nExpiration) );
               rec.push_back( Pair("statusbar", alert.strStatusBar) );
               localAlerts.push_back(rec);
          }
    }
    obj.push_back(Pair("alerts", localAlerts));

    return obj;
}

// #if CLIENT_VERSION_IS_RELEASE != true

Value sendalert(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 7)
        throw runtime_error(
            "sendalert <message> <privatekey> <minver> <maxver> <subvers> <priority> <id> [relaydays] [expiredays] [cancelupto]\n"
            " Sends an Alert message, private command for network administrators only.\n"
            "\nArguments:\n"
            " 1. \"message\"      (string, required)  Is the alert text message.\n"
            " 2. \"privatekey\"   (string, required)  Is hex string of alert private key.\n"
            " 3. minver: n      (numeric, required) Is the minimum applicable protocol version (70006-70009).\n"
            " 4. maxver: n      (numeric, required) Is the maximum applicable protocol version (70006-70009).\n"
            " 5. \"subvers\":     (string, required)  If not null, a specific set of client version(s) (see BIP14 for specs)\n"
            " 6. priority: n    (numeric, required) Is integer priority number (1-65565).\n"
            " 7. id: n          (numeric, required) Is the alert id you have assigned (1-65565).\n"
            " 8. relaydays: n   (numeric, optional) Relay this alert for this many days (1-65565).\n"
            " 9. expiredays: n  (numeric, optional) Expire this alert in this many days (1-65565).\n"
            "10. cancelupto: n  (numeric, optional) Cancels all alert id's up to this number (1-65565).\n"
            "\nResult:\n"
            " Returns JSON result if successful.");
    LOCK(cs_main);

    CAlert alert;
    CKey key;

    alert.strStatusBar = params[0].get_str();
    alert.nMinVer = params[2].get_int();
    alert.nMaxVer = params[3].get_int();

    // We need to parse out the subversion strings as per BIP14 and create a
    // set of matchable versions this alert is targeted for.  A null string
    // indicates all version, 1st we do a small bit of user input verification.
    // So this should work if the string is null, if there is one version string,
    // or more, separated by '/' as per the specification.
    string strSetSubVers = params[4].get_str();
    if( strSetSubVers.size() ) {
        // The 1st and last chars need to be a '/' or it was not entered correctly
        if( strSetSubVers[0] == '/'  && strSetSubVers[ strSetSubVers.size() - 1 ] == '/' ) {
            std::string::size_type iPos = 1, iLast;     // We have at least one likely good string
            while( ( iLast = strSetSubVers.find('/',iPos--) ) != std::string::npos ) {
                alert.setSubVer.insert( strSetSubVers.substr(iPos,++iLast-iPos));
                iPos = iLast;
            }
        } else
            throw runtime_error( "Invalid client subversion(s) string, see BIP14 for specifications\n");
    }
    // else We're done, the set of subver strings defaults to null on creation.
    alert.nPriority = params[5].get_int();
    alert.nID = params[6].get_int();
    if (params.size() > 9)
        alert.nCancel = params[9].get_int();
    alert.nVersion = PROTOCOL_VERSION;

    const int64_t i64AlertNow = GetAdjustedTime();

    alert.nRelayUntil = ( params.size() > 7 ) ? params[7].get_int() : 365;
    alert.nRelayUntil *= 24*60*60;       // One day in seconds
    alert.nRelayUntil += i64AlertNow;

    alert.nExpiration = ( params.size() > 8 ) ? params[8].get_int() : 365;
    alert.nExpiration *= 24*60*60;       // One day in seconds
    alert.nExpiration += i64AlertNow;

    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << (CUnsignedAlert)alert;
    alert.vchMsg = vector<unsigned char>(sMsg.begin(), sMsg.end());
    vector<unsigned char> vchPrivKey = ParseHex(params[1].get_str());
    if( vchPrivKey.size() == 0x20 )
        key.Set( vchPrivKey.begin(), vchPrivKey.end(), false );
    else if( !key.SetPrivKey( CPrivKey(vchPrivKey.begin(), vchPrivKey.end()), false ) )
        throw runtime_error( "Unable to verify alert Private key, check private key?\n");

    if (!key.Sign(Hash(alert.vchMsg.begin(), alert.vchMsg.end()), alert.vchSig))
        throw runtime_error( "Unable to sign alert, check private key?\n");
    else if(!alert.ProcessAlert())
        throw runtime_error( "Failed to process alert.\n");

    // Relay alert to the other nodes
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
            alert.RelayTo(pnode);
    }
// This is not needed here, as we are likely one of the nodes that will process the alert message in main.cpp
#if defined( DONT_COMPILE )
    if( !isMainNetwork() ) {
        //! A Special Alert message can be used to change the TestNet retarget parameters for all the nodes.
        //! All Mining must be briefly stopped, then the alert issued and mining again resumed.  The new
        //! settings will then be used for calculating a retarget difficulty value for all the next blocks
        //! mined.  This causes a complete RetargetPID Controller reset to occur and the new constants
        //! for P-I-D terms loaded, as if the software was just booting up.  No new TestNet miners can be
        //! added to the network as any reload of the blockchain will fail, however it can be useful for
        //! testing different settings on a small controlled TestNet, were those details can be ignored
        //! and the user(s) simply want new values tried and experimented with for a time.
        if( boost::algorithm::istarts_with(alert.strStatusBar, "retargetpid ") ) {
            RetargetPidReset( alert.strStatusBar.substr(11, string::npos), chainActive->Tip() );
        }
    }
#endif

    // At this point, the Anoncoin network will be flooded with the alert message before very much time has passed.
    Object res;
    res.push_back( Pair("alertid", alert.nID) );
    res.push_back( Pair("priority", alert.nPriority) );
    res.push_back( Pair("version", alert.nVersion) );
    res.push_back( Pair("minver", alert.nMinVer) );
    res.push_back( Pair("maxver", alert.nMaxVer) );
    res.push_back( Pair("subver", SingleAlertSubVersionsString(alert.setSubVer)) );
    res.push_back( Pair("relayuntil", alert.nRelayUntil) );
    res.push_back( Pair("expiration", alert.nExpiration) );
    res.push_back( Pair("statusbar", alert.strStatusBar) );
    if (alert.nCancel > 0)
        res.push_back( Pair("cancel", alert.nCancel) );
    return res;
}

Value makekeypair(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "makekeypair [prefix]\n"
            " Make a public/private key pair.\n"
            " [prefix] is optional preferred prefix for the public key.\n");

    string strPrefix = "";
    if (params.size() > 0)
        strPrefix = params[0].get_str();

    CKey key;
    CPubKey pubkey;
    int nCount = 0;
    do
    {
        key.MakeNewKey(false);
        pubkey = key.GetPubKey();
        nCount++;
    } while (nCount < 10000 && strPrefix != HexStr(pubkey.begin(), pubkey.end()).substr(0, strPrefix.size()));

    if (strPrefix != HexStr(pubkey.begin(), pubkey.end()).substr(0, strPrefix.size()))
        return Value::null;

    Object result;
    result.push_back(Pair("publickey", HexStr(pubkey.begin(), pubkey.end())));
    result.push_back(Pair("privatekey", CAnoncoinSecret(key).ToString()));
    return result;
}

// #endif  // !CLIENT_VERSION_IS_RELEASE
