#include "parser/stack_trace_parser.h"

#include <cctype>

#include "utils/class_names.h"
#include "utils/strings.h"

namespace minesleuth {
namespace {

bool equalsIgnoreCase(std::string_view text, std::string_view expected) {
    if (text.size() != expected.size()) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(text[i])) !=
            std::tolower(static_cast<unsigned char>(expected[i]))) {
            return false;
        }
    }
    return true;
}

bool startsWithIgnoreCase(std::string_view text, std::string_view prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }
    return equalsIgnoreCase(text.substr(0, prefix.size()), prefix);
}

bool endsWithIgnoreCase(std::string_view text, std::string_view suffix) {
    if (text.size() < suffix.size()) {
        return false;
    }
    return equalsIgnoreCase(text.substr(text.size() - suffix.size()), suffix);
}

bool isIdentifierChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '$';
}

bool isClassNameSegment(std::string_view name) {
    if (name.empty()) {
        return false;
    }
    if (std::isdigit(static_cast<unsigned char>(name.front())) != 0) {
        return false;
    }
    for (char c : name) {
        if (!isIdentifierChar(c)) {
            return false;
        }
    }
    return true;
}

bool isQualifiedName(std::string_view name) {
    if (name.empty() || name.front() == '.' || name.back() == '.') {
        return false;
    }
    std::size_t dots = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= name.size(); ++i) {
        const bool atEnd = i == name.size();
        if (atEnd || name[i] == '.') {
            if (i == start) {
                return false;
            }
            if (!isClassNameSegment(name.substr(start, i - start))) {
                return false;
            }
            start = i + 1;
            if (!atEnd) {
                ++dots;
            }
        }
    }
    if (dots >= 1) {
        return true;
    }
    return endsWithIgnoreCase(name, "Exception") || endsWithIgnoreCase(name, "Error") ||
           endsWithIgnoreCase(name, "Throwable") || endsWithIgnoreCase(name, "Failure") ||
           endsWithIgnoreCase(name, "Problem");
}

std::string_view stripThreadPrefix(std::string_view text) {
    if (!startsWithIgnoreCase(text, "Exception in thread ")) {
        return text;
    }
    const std::size_t quoteStart = text.find('"');
    if (quoteStart == std::string_view::npos) {
        return text;
    }
    const std::size_t quoteEnd = text.find('"', quoteStart + 1);
    if (quoteEnd == std::string_view::npos) {
        return text;
    }
    return strings::trimView(text.substr(quoteEnd + 1));
}

std::string_view stripFrameMetadata(std::string_view frame) {
    // Spigot and older Bukkit servers append metadata such as
    // "~[minecraft_server.jar:...]" or "[?:1.8.0_144]" after the frame.
    const std::size_t close = frame.rfind(')');
    if (close == std::string_view::npos) {
        return frame;
    }
    std::string_view tail = strings::trimView(frame.substr(close + 1));
    if (!tail.empty() && tail.front() == '~') {
        tail.remove_prefix(1);
    }
    if (tail.size() >= 2 && tail.front() == '[' && tail.back() == ']') {
        return frame.substr(0, close + 1);
    }
    return frame;
}

}  // namespace

bool StackTraceParser::parseExceptionHeader(std::string_view line, TraceSegment& out) {
    std::string_view text = strings::trimView(line);
    text = stripThreadPrefix(text);
    if (text.empty()) {
        return false;
    }

    TraceSegmentKind kind = TraceSegmentKind::Root;
    if (startsWithIgnoreCase(text, "Caused by:")) {
        kind = TraceSegmentKind::Cause;
        text = strings::trimView(text.substr(10));
    } else if (startsWithIgnoreCase(text, "Suppressed:")) {
        kind = TraceSegmentKind::Suppressed;
        text = strings::trimView(text.substr(11));
    }

    const std::size_t colon = text.find(':');
    const std::string_view classPart = strings::trimView(colon == std::string_view::npos
                                                             ? text
                                                             : text.substr(0, colon));
    if (!isQualifiedName(classPart)) {
        return false;
    }

    out.kind = kind;
    out.exceptionClass = std::string(classPart);
    out.message = colon == std::string_view::npos ? std::string{}
                                                  : strings::trim(text.substr(colon + 1));
    out.headerRaw = strings::trim(line);
    return true;
}

bool StackTraceParser::looksLikeExceptionHeader(std::string_view line) {
    TraceSegment probe;
    return parseExceptionHeader(line, probe);
}

bool StackTraceParser::looksLikeFrame(std::string_view line) {
    const std::string_view text = strings::trimView(line);
    if (!strings::startsWith(text, "at ")) {
        return false;
    }
    const std::string_view rest = stripFrameMetadata(strings::trimView(text.substr(3)));
    if (rest.empty() || rest.back() != ')') {
        return false;
    }
    const std::size_t open = rest.rfind('(');
    if (open == std::string_view::npos || open == 0) {
        return false;
    }
    const std::string_view head = strings::trimView(rest.substr(0, open));
    const std::size_t lastDot = head.rfind('.');
    if (lastDot == std::string_view::npos || lastDot == 0 || lastDot + 1 >= head.size()) {
        return false;
    }
    for (char c : head) {
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
            return false;
        }
    }
    const std::string_view classPart = head.substr(0, lastDot);
    const std::string_view methodPart = head.substr(lastDot + 1);
    // The module/class-loader prefix may contain version and loader notation
    // ("example.module@1.0", "fabric-loader"), but the class name itself is a
    // normal dotted binary name.
    const std::string_view className = strings::classNameWithoutModule(classPart);
    if (className.empty()) {
        return false;
    }
    for (char c : className) {
        if (!isIdentifierChar(c) && c != '.') {
            return false;
        }
    }
    for (char c : methodPart) {
        if (!isIdentifierChar(c) && c != '<' && c != '>') {
            return false;
        }
    }
    return true;
}

bool StackTraceParser::parseOmission(std::string_view line, int& omittedFrames) {
    std::string_view text = strings::trimView(line);
    if (!strings::startsWith(text, "...")) {
        return false;
    }
    text = strings::trimView(text.substr(3));
    std::size_t digits = 0;
    while (digits < text.size() && std::isdigit(static_cast<unsigned char>(text[digits])) != 0) {
        ++digits;
    }
    if (digits == 0 || digits > 9) {
        return false;
    }
    std::size_t value = 0;
    for (std::size_t i = 0; i < digits; ++i) {
        value = value * 10 + static_cast<std::size_t>(text[i] - '0');
    }
    if (!strings::trimView(text.substr(digits)).empty() &&
        strings::trimView(text.substr(digits)) != "more") {
        return false;
    }
    omittedFrames = static_cast<int>(value);
    return true;
}

StackFrame StackTraceParser::parseFrame(std::string_view line) {
    StackFrame frame;
    frame.raw = strings::trim(line);

    const std::string_view text = strings::trimView(line);
    if (!strings::startsWith(text, "at ")) {
        return frame;
    }
    const std::string_view rest = stripFrameMetadata(strings::trimView(text.substr(3)));
    if (rest.empty() || rest.back() != ')') {
        return frame;
    }
    const std::size_t open = rest.rfind('(');
    if (open == std::string_view::npos) {
        return frame;
    }

    const std::string_view head = strings::trimView(rest.substr(0, open));
    const std::string_view source =
        strings::trimView(rest.substr(open + 1, rest.size() - open - 2));

    const std::size_t lastDot = head.rfind('.');
    if (lastDot != std::string_view::npos && lastDot > 0 && lastDot + 1 < head.size()) {
        frame.className =
            std::string(strings::classNameWithoutModule(head.substr(0, lastDot)));
        frame.methodName = std::string(head.substr(lastDot + 1));
    } else if (!head.empty()) {
        frame.className = std::string(strings::classNameWithoutModule(head));
    }

    if (source == "Native Method") {
        frame.nativeMethod = true;
        return frame;
    }
    if (source.empty() || source == "Unknown Source") {
        if (!source.empty()) {
            frame.sourceFile = std::string(source);
        }
        return frame;
    }

    const std::size_t colon = source.rfind(':');
    if (colon == std::string_view::npos) {
        frame.sourceFile = std::string(source);
        return frame;
    }
    const std::string_view suffix = source.substr(colon + 1);
    bool numeric = !suffix.empty();
    for (char c : suffix) {
        if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
            numeric = false;
            break;
        }
    }
    if (!numeric) {
        frame.sourceFile = std::string(source);
        return frame;
    }
    frame.sourceFile = std::string(source.substr(0, colon));
    int lineNumber = 0;
    for (char c : suffix) {
        lineNumber = lineNumber * 10 + (c - '0');
    }
    frame.lineNumber = lineNumber;
    return frame;
}

void StackTraceBuilder::reset() {
    segments_.clear();
    rawLines_.clear();
    startLine_ = 0;
    endLine_ = 0;
    causeCount_ = 0;
    suppressedCount_ = 0;
}

void StackTraceBuilder::appendRaw(std::string_view line, std::size_t lineNumber) {
    rawLines_.emplace_back(line);
    if (startLine_ == 0) {
        startLine_ = lineNumber;
    }
    endLine_ = lineNumber;
}

void StackTraceBuilder::appendSegment(TraceSegment segment, std::string_view sourceLine,
                                      std::size_t lineNumber) {
    if (segment.kind == TraceSegmentKind::Cause) {
        segment.depth = causeCount_++;
    } else if (segment.kind == TraceSegmentKind::Suppressed) {
        segment.depth = suppressedCount_++;
    }
    segments_.push_back(std::move(segment));
    appendRaw(sourceLine, lineNumber);
}

void StackTraceBuilder::seedRoot(std::string_view headerLine, std::size_t lineNumber,
                                 bool includeHeaderInRawText) {
    TraceSegment segment;
    if (!StackTraceParser::parseExceptionHeader(headerLine, segment)) {
        segment.kind = TraceSegmentKind::Root;
        segment.exceptionClass = strings::trim(headerLine);
    }
    if (segment.kind == TraceSegmentKind::Cause) {
        segment.depth = causeCount_++;
    } else if (segment.kind == TraceSegmentKind::Suppressed) {
        segment.depth = suppressedCount_++;
    }
    if (segment.headerRaw.empty()) {
        segment.headerRaw = strings::trim(headerLine);
    }
    segments_.push_back(std::move(segment));

    if (includeHeaderInRawText) {
        appendRaw(strings::trimView(headerLine), lineNumber);
        return;
    }
    if (startLine_ == 0) {
        startLine_ = lineNumber;
    }
    endLine_ = lineNumber;
}

bool StackTraceBuilder::accept(std::string_view line, std::size_t lineNumber) {
    TraceSegment parsed;
    if (StackTraceParser::parseExceptionHeader(line, parsed)) {
        if (parsed.kind == TraceSegmentKind::Root) {
            return false;
        }
        appendSegment(std::move(parsed), strings::trimView(line), lineNumber);
        return true;
    }

    if (segments_.empty()) {
        return false;
    }

    int omitted = 0;
    if (StackTraceParser::parseOmission(line, omitted)) {
        segments_.back().omittedFrames += omitted;
        appendRaw(line, lineNumber);
        return true;
    }

    if (StackTraceParser::looksLikeFrame(line)) {
        segments_.back().frames.push_back(StackTraceParser::parseFrame(line));
        appendRaw(line, lineNumber);
        return true;
    }

    return false;
}

StackTrace StackTraceBuilder::finish() {
    StackTrace trace;
    trace.segments = std::move(segments_);
    trace.startLine = startLine_;
    trace.endLine = endLine_;
    for (std::size_t i = 0; i < rawLines_.size(); ++i) {
        if (i > 0) {
            trace.rawText.push_back('\n');
        }
        trace.rawText += rawLines_[i];
    }
    reset();
    return trace;
}

}  // namespace minesleuth
