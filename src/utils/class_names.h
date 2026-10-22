#pragma once

#include <cstddef>
#include <string_view>

namespace minesleuth::strings {

// Java stack frames may prefix the binary class name with a module and/or class
// loader: "java.base/java.lang.Thread", "knot//net.minecraft.class_1439", or
// "app/loader@1.0/com.example.Mod". Stack traces use dotted binary class names,
// so the final slash-separated segment is the class name.
inline std::string_view classNameWithoutModule(std::string_view className) {
    const std::size_t slash = className.rfind('/');
    return slash == std::string_view::npos ? className : className.substr(slash + 1);
}

}  // namespace minesleuth::strings
