#pragma once

#include <QString>
#include <QVector>

namespace OpenMix {

// enum of all supported console types
enum class ConsoleType {
    X32,  // X32 series (X32, Compact, Producer, Rack, Core)
    M32,  // Midas M32 series (M32, LIVE, R, R LIVE, C)
    Wing, // Behringer WING / WING Compact

    SQ5,     // SQ-5
    SQ6,     // SQ-6
    SQ7,     // SQ-7
    GLD80,   // GLD-80
    GLD112,  // GLD-112
    Avantis, // Avantis / Avantis Solo
    DLive,   // dLive (DM0-64, CDM32-64)

    TF1, // TF1
    TF3, // TF3
    TF5, // TF5
    QL1, // QL1
    QL5, // QL5
    CL1, // CL1
    CL3, // CL3
    CL5, // CL5
    DM7, // DM7 / DM7 Compact

    SD7,  // DiGiCo SD7
    SD9,  // DiGiCo SD9
    SD11, // DiGiCo SD11
    SD12, // DiGiCo SD12

    Loopback, // loopback for testing

    Unknown
};

// protocol type used for communication
enum class ProtocolType {
    OscUdp,    // OSC over UDP (X32, WING)
    MidiTcp,   // MIDI over TCP (SQ, GLD)
    BinaryTcp, // custom binary TCP (Avantis, dLive)
    TextTcp,   // text-based TCP (Yamaha TF/QL/CL/DM7)
    Internal   // internal loopback (no network)
};

// manufacturer categories
enum class Manufacturer { Behringer, Midas, AllenHeath, Yamaha, DiGiCo, Unknown };

// capabilities descriptor for each console type
struct MixerCapabilities {
    static constexpr int DEFAULT_PORT = 10023;
    static constexpr int DEFAULT_DCA_COUNT = 8;
    static constexpr int DEFAULT_INPUT_CHANNELS = 32;
    static constexpr int DEFAULT_MIX_BUSES = 16;
    static constexpr int DEFAULT_SCENES = 100;
    static constexpr int DEFAULT_MAX_DCA_NAME_LENGTH = 6;

    ConsoleType type = ConsoleType::Unknown;
    Manufacturer manufacturer = Manufacturer::Unknown;
    ProtocolType protocol = ProtocolType::OscUdp;

    QString displayName; // human-readable name
    QString protocolId;  // ID for factory lookup ("x32", "wing", "sq", etc.)

    int defaultPort = DEFAULT_PORT;
    int dcaCount = DEFAULT_DCA_COUNT;
    int inputChannels = DEFAULT_INPUT_CHANNELS;
    int mixBuses = DEFAULT_MIX_BUSES;
    int matrixOutputs = 0;
    int scenes = DEFAULT_SCENES;
    int maxDCANameLength = DEFAULT_MAX_DCA_NAME_LENGTH;

    bool supportsSceneRecall = true;
    bool supportsDCAMute = true;
    bool supportsDCAFader = true;

    // EQ capabilities
    int eqBandsPerChannel = 0; // 0 = no EQ, 4 or 6 typical
    bool supportsChannelEQ = false;
    QVector<QString> eqBandTypes; // supported band types per console

    // FX capabilities
    int effectSendBuses = 0; // # of effect send buses
    bool supportsEffectSends = false;

    [[nodiscard]] static MixerCapabilities forConsole(ConsoleType type);
    [[nodiscard]] static MixerCapabilities forProtocolId(const QString& protocolId);
    [[nodiscard]] static QVector<MixerCapabilities> allSupported();
    [[nodiscard]] static QVector<MixerCapabilities> forManufacturer(Manufacturer manufacturer);

    [[nodiscard]] bool isSupported() const;
    [[nodiscard]] QString manufacturerName() const;
};

// convert between types & strings
QString consoleTypeToString(ConsoleType type);
ConsoleType stringToConsoleType(const QString& str);
QString protocolTypeToString(ProtocolType type);

} // namespace OpenMix
