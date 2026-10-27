#include "dbc.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>

namespace {

int failures = 0;

void check(bool condition, const char* what)
{
    if (!condition) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

bool near(double a, double b)
{
    return std::fabs(a - b) < 1e-6;
}

QByteArray bytes(std::initializer_list<int> values)
{
    QByteArray out;
    for (const int value : values) {
        out.append(static_cast<char>(value));
    }
    return out;
}

const char* sample = R"(VERSION ""

NS_ :
	NS_DESC_
	CM_
	BA_DEF_
	VAL_
	SIG_GROUP_

BS_:

BU_: ECU TOOL

BO_ 100 Engine: 8 ECU
 SG_ EngineSpeed : 0|16@1+ (0.25,0) [0|16383.75] "rpm" TOOL
 SG_ Gear : 16|2@1+ (1,0) [0|3] "" TOOL
 SG_ Temp : 31|8@0- (1,-40) [-168|87] "degC" TOOL
 SG_ Pressure : 39|12@0+ (0.1,0) [0|409.5] "kPa" TOOL

BO_ 200 Muxed: 2 ECU
 SG_ Selector M : 0|8@1+ (1,0) [0|255] "" TOOL
 SG_ ValueA m0 : 8|8@1+ (1,0) [0|255] "" TOOL
 SG_ ValueB m1 : 8|8@1+ (2,0) [0|510] "" TOOL

BO_ 2566844672 J1939Msg: 8 ECU
 SG_ WheelSpeed : 8|16@1+ (0.00390625,0) [0|250.996] "km/h" TOOL

BO_ 3221225472 VECTOR__INDEPENDENT_SIG_MSG: 0 Vector__XXX
 SG_ Orphan : 0|8@1+ (1,0) [0|0] "" Vector__XXX

CM_ BO_ 100 "Engine status";
CM_ SG_ 100 Gear "Selected gear;
spans two lines";
BA_DEF_ BO_ "GenMsgCycleTime" INT 0 65535;
VAL_ 100 Gear 0 "Park" 1 "Drive" 2 "Reverse" ;
)";

void testParse(const Dbc::Database& db)
{
    check(db.messages().size() == 3, "three messages (placeholder removed)");
    check(db.signalCount() == 8, "eight signals");

    const int engine = db.findMessage(100, false, false);
    check(engine >= 0, "engine message found");
    if (engine < 0) {
        return;
    }
    const Dbc::Message& message = db.messages()[engine];
    check(message.name == "Engine" && message.length == 8 && message.transmitter == "ECU", "engine header");
    check(message.comment == "Engine status", "message comment");
    const Dbc::Signal& gear = message.signalList[1];
    check(gear.comment == "Selected gear;\nspans two lines", "multi-line signal comment with ';' inside");
    check(gear.valueNames.value(1) == "Drive", "value table");
    check(!message.signalList[2].littleEndian && message.signalList[2].isSigned, "Motorola signed flags");
}

void testDecode(const Dbc::Database& db)
{
    const Dbc::Message& message = db.messages()[db.findMessage(100, false, false)];
    const auto values = Dbc::decode(message, bytes({0x10, 0x27, 0x01, 0xF6, 0xAB, 0xC0, 0x00, 0x00}));
    check(values.size() == 4, "four engine signals decoded");
    if (values.size() != 4) {
        return;
    }
    check(near(values[0].physical, 2500.0), "Intel 16-bit with factor");
    check(Dbc::formatValue(message.signalList[1], values[1]) == "Drive", "value name formatting");
    check(values[2].raw == -10 && near(values[2].physical, -50.0), "Motorola signed 8-bit with offset");
    check(values[3].raw == 0xABC && near(values[3].physical, 274.8), "Motorola 12-bit across bytes");
    check(Dbc::formatValue(message.signalList[3], values[3]) == "274.8", "decimals follow factor");

    const auto shortFrame = Dbc::decode(message, bytes({0x10, 0x27}));
    check(shortFrame.size() == 1, "signals beyond a short payload are skipped");
}

void testMultiplex(const Dbc::Database& db)
{
    const Dbc::Message& message = db.messages()[db.findMessage(200, false, false)];
    const auto selectOne = Dbc::decode(message, bytes({0x01, 0x55}));
    check(selectOne.size() == 2, "multiplexor plus one selected signal");
    if (selectOne.size() == 2) {
        check(message.signalList[selectOne[1].signalIndex].name == "ValueB" && near(selectOne[1].physical, 170.0), "selected multiplexed signal");
    }
    const auto selectZero = Dbc::decode(message, bytes({0x00, 0x07}));
    check(selectZero.size() == 2 && message.signalList[selectZero[1].signalIndex].name == "ValueA", "other multiplex branch");
}

void testExtendedAndJ1939(const Dbc::Database& db)
{
    check(db.findMessage(0x18FEF100, true, false) >= 0, "extended ID exact match");
    check(db.findMessage(0x18FEF100, false, false) < 0, "standard frame does not match extended definition");
    check(db.findMessage(0x18FEF117, true, false) < 0, "other source address needs J1939 match");
    check(db.findMessage(0x18FEF117, true, true) >= 0, "J1939 match ignores source address");
}

}

int main()
{
    Dbc::Database db;
    QString error;
    check(db.parse(QString::fromUtf8(sample), &error), "sample parses");
    testParse(db);
    if (failures == 0) {
        testDecode(db);
        testMultiplex(db);
        testExtendedAndJ1939(db);
    }

    Dbc::Database empty;
    check(!empty.parse("VERSION \"\"\n", &error) && !error.isEmpty(), "file without messages is rejected");

    if (failures == 0) {
        std::printf("All DBC tests passed.\n");
    }
    return failures == 0 ? 0 : 1;
}
