#pragma once

#include <QColor>
#include <QFont>
#include <QString>

namespace OpenMix {
namespace Theme {

namespace Colors {

constexpr const char* BgBase = "#131416";
constexpr const char* BgPrimary = "#18191c";
constexpr const char* BgSurface = "#1e2024";
constexpr const char* BgElevated = "#252830";
constexpr const char* BgHover = "#2c3038";
constexpr const char* BgActive = "#363b44";
constexpr const char* BgInput = "#1a1c20";
constexpr const char* BgInputHover = "#22252a";

constexpr const char* TextPrimary = "#f0f1f3";
constexpr const char* TextSecondary = "#9ca3af";
constexpr const char* TextTertiary = "#6b7280";
constexpr const char* TextMuted = "#4b5563";
constexpr const char* TextDisabled = "#374151";

constexpr const char* AccentGreen = "#22c55e";
constexpr const char* AccentGreenHover = "#4ade80";
constexpr const char* AccentGreenMuted = "#166534";
constexpr const char* AccentGreenBg = "rgba(34, 197, 94, 0.12)";

constexpr const char* AccentBlue = "#3b82f6";
constexpr const char* AccentBlueHover = "#60a5fa";
constexpr const char* AccentBlueMuted = "#1e40af";
constexpr const char* AccentBlueBg = "rgba(59, 130, 246, 0.12)";

constexpr const char* AccentAmber = "#f59e0b";
constexpr const char* AccentAmberHover = "#fbbf24";
constexpr const char* AccentAmberMuted = "#92400e";
constexpr const char* AccentAmberBg = "rgba(245, 158, 11, 0.12)";

constexpr const char* AccentRed = "#ef4444";
constexpr const char* AccentRedHover = "#f87171";
constexpr const char* AccentRedMuted = "#991b1b";
constexpr const char* AccentRedBg = "rgba(239, 68, 68, 0.12)";

constexpr const char* BorderSubtle = "#262931";
constexpr const char* BorderDefault = "#2e323b";
constexpr const char* BorderStrong = "#3d4451";
constexpr const char* BorderFocus = "#3b82f6";

constexpr const char* ShadowColor = "rgba(0, 0, 0, 0.5)";
constexpr const char* ShadowColorLight = "rgba(0, 0, 0, 0.25)";

} // namespace Colors

constexpr const char* BgPrimary = Colors::BgPrimary;
constexpr const char* BgSurface = Colors::BgSurface;
constexpr const char* BgElevated = Colors::BgElevated;
constexpr const char* BgHover = Colors::BgHover;
constexpr const char* BgInput = Colors::BgInput;
constexpr const char* TextPrimary = Colors::TextPrimary;
constexpr const char* TextSecondary = Colors::TextSecondary;
constexpr const char* TextMuted = Colors::TextTertiary;
constexpr const char* TextDisabled = Colors::TextDisabled;
constexpr const char* AccentGreen = Colors::AccentGreen;
constexpr const char* AccentGreenHover = Colors::AccentGreenHover;
constexpr const char* AccentBlue = Colors::AccentBlue;
constexpr const char* AccentBlueHover = Colors::AccentBlueHover;
constexpr const char* AccentAmber = Colors::AccentAmber;
constexpr const char* AccentAmberHover = Colors::AccentAmberHover;
constexpr const char* AccentRed = Colors::AccentRed;
constexpr const char* AccentRedHover = Colors::AccentRedHover;
constexpr const char* Border = Colors::BorderDefault;
constexpr const char* BorderLight = Colors::BorderStrong;
constexpr const char* BorderFocus = Colors::BorderFocus;

namespace Spacing {
constexpr int XXS = 2;
constexpr int XS = 4;
constexpr int S = 8;
constexpr int M = 12;
constexpr int L = 16;
constexpr int XL = 24;
constexpr int XXL = 32;
} // namespace Spacing

constexpr int SpacingXXS = Spacing::XXS;
constexpr int SpacingXS = Spacing::XS;
constexpr int SpacingS = Spacing::S;
constexpr int SpacingM = Spacing::M;
constexpr int SpacingL = Spacing::L;
constexpr int SpacingXL = Spacing::XL;

namespace Radius {
constexpr int None = 0;
constexpr int XS = 2;
constexpr int S = 4;
constexpr int M = 6;
constexpr int L = 8;
constexpr int XL = 12;
constexpr int Full = 9999;
} // namespace Radius

constexpr int RadiusS = Radius::S;
constexpr int RadiusM = Radius::M;
constexpr int RadiusL = Radius::L;
constexpr int RadiusXL = Radius::XL;

namespace Animation {
constexpr int Instant = 50;
constexpr int Fast = 100;
constexpr int Normal = 150;
constexpr int Slow = 250;
constexpr int Smooth = 300;
} // namespace Animation

constexpr int AnimFast = Animation::Fast;
constexpr int AnimNormal = Animation::Normal;
constexpr int AnimSlow = Animation::Slow;

namespace Font {
constexpr int SizeXS = 10;
constexpr int SizeS = 11;
constexpr int SizeBase = 12;
constexpr int SizeM = 13;
constexpr int SizeL = 14;
constexpr int SizeXL = 16;
constexpr int SizeXXL = 18;
constexpr int Size3XL = 24;

constexpr int WeightNormal = 400;
constexpr int WeightMedium = 500;
constexpr int WeightSemiBold = 600;
constexpr int WeightBold = 700;
} // namespace Font

constexpr int FontSizeSmall = Font::SizeS;
constexpr int FontSizeNormal = Font::SizeBase;
constexpr int FontSizeLarge = Font::SizeL;
constexpr int FontSizeXLarge = Font::SizeXL;

namespace Size {
constexpr int ControlHeightS = 24;
constexpr int ControlHeightM = 28;
constexpr int ControlHeightL = 32;
constexpr int ControlHeightXL = 36;
constexpr int IconS = 14;
constexpr int IconM = 16;
constexpr int IconL = 20;
constexpr int IconXL = 24;
} // namespace Size

// base dark theme; pass highContrast=true for a brighter booth-friendly variant
QString globalStylesheet(bool highContrast = false);

QColor color(const char* themeColor);

QColor withAlpha(const char* themeColor, int alpha);
QColor withAlpha(const QColor& color, int alpha);

QFont monoFont(int size = Font::SizeBase);

QFont headerFont(int size = Font::SizeL);

QFont uiFont(int size = Font::SizeBase, int weight = Font::WeightNormal);

namespace ObjectNames {
constexpr const char* BubbleBar = "BubbleBar";
constexpr const char* BubbleButton = "BubbleButton";
constexpr const char* PopOutWindow = "PopOutWindow";
constexpr const char* PopOutTitleBar = "PopOutTitleBar";
constexpr const char* PopOutContent = "PopOutContent";
constexpr const char* TimelineToggle = "TimelineToggle";
constexpr const char* TransportBar = "TransportBar";
constexpr const char* StatusIndicator = "StatusIndicator";
constexpr const char* CueListView = "CueListView";
constexpr const char* CueEditor = "CueEditor";
constexpr const char* ParameterWidget = "ParameterWidget";
constexpr const char* CategoryHeader = "CategoryHeader";
constexpr const char* DCAWidget = "DCAWidget";
constexpr const char* GoButton = "GoButton";
constexpr const char* StopButton = "StopButton";
constexpr const char* PanicButton = "PanicButton";
constexpr const char* ConnectionStatus = "ConnectionStatus";
} // namespace ObjectNames

namespace Properties {
constexpr const char* Active = "active";
constexpr const char* Modified = "modified";
constexpr const char* Preview = "preview";
constexpr const char* Connected = "connected";
constexpr const char* Error = "error";
constexpr const char* Muted = "muted";
constexpr const char* Current = "current";
constexpr const char* Standby = "standby";
constexpr const char* Role = "role";
} // namespace Properties

} // namespace Theme
} // namespace OpenMix
