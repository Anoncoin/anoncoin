#include "uritests.h"

#include "guiutil.h"
#include "walletmodel.h"

#include <QUrl>

void URITests::uriTests()
{
    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("anoncoin:AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3?req-dontexist="));
    QVERIFY(!GUIUtil::parseAnoncoinURI(uri, &rv));

    uri.setUrl(QString("anoncoin:AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3?dontexist="));
    QVERIFY(GUIUtil::parseAnoncoinURI(uri, &rv));
    QVERIFY(rv.address == QString("AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("anoncoin:AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3?label=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseAnoncoinURI(uri, &rv));
    QVERIFY(rv.address == QString("AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3"));
    QVERIFY(rv.label == QString("Wikipedia Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("anoncoin:AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3?amount=0.001"));
    QVERIFY(GUIUtil::parseAnoncoinURI(uri, &rv));
    QVERIFY(rv.address == QString("AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("anoncoin:AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3?amount=1.001"));
    QVERIFY(GUIUtil::parseAnoncoinURI(uri, &rv));
    QVERIFY(rv.address == QString("AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("anoncoin:AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3?amount=100&label=Wikipedia Example"));
    QVERIFY(GUIUtil::parseAnoncoinURI(uri, &rv));
    QVERIFY(rv.address == QString("AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Wikipedia Example"));

    uri.setUrl(QString("anoncoin:AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3?message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseAnoncoinURI(uri, &rv));
    QVERIFY(rv.address == QString("AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseAnoncoinURI("anoncoin://AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3?message=Wikipedia Example Address", &rv));
    QVERIFY(rv.address == QString("AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3"));
    QVERIFY(rv.label == QString());

    uri.setUrl(QString("anoncoin:AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3?req-message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseAnoncoinURI(uri, &rv));

    uri.setUrl(QString("anoncoin:AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3?amount=1,000&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseAnoncoinURI(uri, &rv));

    uri.setUrl(QString("anoncoin:AH6PLNDHFeNAngkjkeLhbDsFZQTYFH94i3?amount=1,000.0&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseAnoncoinURI(uri, &rv));
}
