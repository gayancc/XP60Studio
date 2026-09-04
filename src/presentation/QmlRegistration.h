#pragma once

namespace xp60studio::presentation {

// Import URI used by QML: `import XP60Studio.Presentation`
inline constexpr const char* kPresentationModuleUri = "XP60Studio.Presentation";

// Registers the presentation types with the QML type system. Instances are
// created by C++ (main / test setup) and handed to QML as properties; QML can
// only refer to the types, never construct them.
void registerQmlTypes();

} // namespace xp60studio::presentation
