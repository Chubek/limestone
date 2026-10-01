#include "SExprTk.hpp"
#include <array>
#include <iostream>

// XAS over datagrams: parse a document, encode each event into a
// protocol datagram (the wire format declared in SExprTk-XASEvent.h),
// then decode each datagram back into an event — exactly what a UDP
// receiver on the other end would do.
int main() {
    sexprtk::SExprTk rt;
    sexprtk::XASEventDispatcher disp;
    auto cartable = rt.parse(
        sexprtk::Source::from_string("(hello (world 42))"), &disp);

    std::cout << "events: " << disp.size() << "\n";
    for (const auto& ev : disp.buffered) {
        // Encode into a datagram frame via the C protocol interface.
        auto c_event = ev.to_c(/*source_id=*/1);
        std::array<unsigned char, SEXPRTK_XAS_MAX_DATAGRAM> storage;
        sexprtk_xas_frame frame;
        sexprtk_xas_frame_init(&frame);
        frame.bytes = storage.data();
        frame.capacity = storage.size();

        if (sexprtk_xas_frame_encode(&c_event, &frame) != SEXPRTK_XAS_OK) {
            std::cerr << "encode failed for seq " << ev.sequence << '\n';
            continue;
        }

        // Decode it back, as the receiving process would.
        sexprtk_xas_event decoded;
        if (sexprtk_xas_frame_decode(&frame, &decoded) != SEXPRTK_XAS_OK) {
            std::cerr << "decode failed for seq " << ev.sequence << '\n';
            continue;
        }

        std::cout << decoded.sequence << "|"
                  << sexprtk_xas_event_kind_name(decoded.kind) << "|"
                  << (decoded.payload
                          ? std::string(decoded.payload, decoded.payload_length)
                          : std::string{})
                  << "  frame_bytes=" << frame.length
                  << "  line=" << decoded.line
                  << "  col=" << decoded.column << '\n';
    }
}
