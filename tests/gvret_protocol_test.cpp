#include "gvret_protocol.h"

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

QByteArray bytes(std::initializer_list<int> values)
{
    QByteArray out;
    for (const int value : values) {
        out.append(static_cast<char>(value));
    }
    return out;
}

void testEncodeFrame()
{
    const QByteArray standard = gvret::encodeFrame(0x123, false, 0, bytes({0xAA, 0xBB}));
    check(standard == bytes({0xF1, 0x00, 0x23, 0x01, 0x00, 0x00, 0x00, 0x02, 0xAA, 0xBB, 0x00}), "standard frame encoding");

    const QByteArray extended = gvret::encodeFrame(0x18FEF100, true, 1, {});
    check(extended == bytes({0xF1, 0x00, 0x00, 0xF1, 0xFE, 0x98, 0x01, 0x00, 0x00}), "extended frame encoding");
}

void testSetupBus()
{
    // 500000 = 0x0007A120; valid | enabled = 0xC0000000.
    check(gvret::setupBus(0, 500000, false) == bytes({0xF1, 0x05, 0x20, 0xA1, 0x07, 0xC0, 0, 0, 0, 0, 0}), "bus 0 normal");
    // Listen-only adds 0x20000000.
    check(gvret::setupBus(0, 250000, true) == bytes({0xF1, 0x05, 0x90, 0xD0, 0x03, 0xE0, 0, 0, 0, 0, 0}), "bus 0 listen-only");
    check(gvret::setupBus(1, 125000, false) == bytes({0xF1, 0x05, 0, 0, 0, 0, 0x48, 0xE8, 0x01, 0xC0, 0}), "bus 1 normal");
}

void testParseFrames()
{
    gvret::Parser parser;
    gvret::ParseResult result;
    // Boot noise, a standard frame, a zero-length extended frame on bus 1.
    const QByteArray stream = QByteArray("ESP-ROM boot\r\n")
        + bytes({0xF1, 0x00, 0x10, 0x00, 0x00, 0x00, 0x23, 0x01, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03})
        + bytes({0xF1, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0xF1, 0xFE, 0x98, 0x10});

    // Feed one byte at a time to cover split serial reads.
    for (const char value : stream) {
        parser.feed(QByteArray(1, value), result);
    }

    check(result.frames.size() == 2, "two frames parsed");
    if (result.frames.size() == 2) {
        const auto& first = result.frames[0];
        check(first.timestampUs == 0x10 && first.id == 0x123 && !first.extended && first.bus == 0, "first frame header");
        check(first.data == bytes({0x01, 0x02, 0x03}), "first frame data");
        const auto& second = result.frames[1];
        check(second.id == 0x18FEF100 && second.extended && second.bus == 1 && second.data.isEmpty(), "second frame header");
    }
}

void testParseReplies()
{
    gvret::Parser parser;
    gvret::ParseResult result;
    // Keep-alive with trailing DE AD, device info, then bus params:
    // enabled + listen-only, 500000 bit/s, bus 1 disabled.
    parser.feed(bytes({0xF1, 0x09, 0xDE, 0xAD}), result);
    parser.feed(bytes({0xF1, 0x07, 0x2A, 0x01, 0, 0, 0, 0}), result);
    parser.feed(bytes({0xF1, 0x06, 0x11, 0x20, 0xA1, 0x07, 0x00, 0x00, 0, 0, 0, 0}), result);
    // A frame after the replies proves the parser resynchronised.
    parser.feed(bytes({0xF1, 0x00, 0, 0, 0, 0, 0x55, 0, 0, 0, 0x01, 0x99}), result);

    check(result.deviceInfo && result.deviceInfo->buildNumber == 0x012A, "device info build number");
    check(result.busParams && result.busParams->enabled && result.busParams->listenOnly, "bus params flags");
    check(result.busParams && result.busParams->bitrate == 500000, "bus params bitrate");
    check(result.frames.size() == 1 && result.frames[0].id == 0x55 && result.frames[0].data == bytes({0x99}), "frame after replies");
}

void testRejectsOversizedLength()
{
    gvret::Parser parser;
    gvret::ParseResult result;
    parser.feed(bytes({0xF1, 0x00, 0, 0, 0, 0, 0x01, 0, 0, 0, 0x0F}), result);
    parser.feed(bytes({0xF1, 0x00, 0, 0, 0, 0, 0x02, 0, 0, 0, 0x00}), result);
    check(result.frames.size() == 1 && result.frames[0].id == 0x02, "oversized length dropped and parser recovers");
}

}

int main()
{
    testEncodeFrame();
    testSetupBus();
    testParseFrames();
    testParseReplies();
    testRejectsOversizedLength();

    if (failures == 0) {
        std::printf("All GVRET protocol tests passed.\n");
    }
    return failures == 0 ? 0 : 1;
}
