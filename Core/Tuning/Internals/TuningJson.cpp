#include "Tuning/Internals/TuningJson.h"
#include <string>
#include "Tuning/TuningFile.h"

namespace {
constexpr int MaxDepth = 32;

class Reader {
public:
    explicit Reader(std::string_view text)
        : _text(text) {}

    TuningJsonNode Document() {
        TuningJsonNode root = Value(0);
        SkipSpace();
        if (_at != _text.size()) Fail("unexpected text after the JSON value");
        return root;
    }

private:
    [[noreturn]] void Fail(const std::string& text) const { throw TuningFormatError(_line, text); }

    [[nodiscard]] char Peek() const { return _at < _text.size() ? _text[_at] : '\0'; }

    void SkipSpace() {
        while (_at < _text.size()) {
            const char c = _text[_at];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
            if (c == '\n') ++_line;
            ++_at;
        }
    }

    void Expect(char c) {
        if (Peek() != c) Fail(std::string("expected '") + c + "'");
        ++_at;
    }

    TuningJsonNode Value(int depth) {
        if (depth > MaxDepth) Fail("nesting is too deep");
        SkipSpace();
        TuningJsonNode node;
        node.line = _line;
        node.begin = _at;
        const char c = Peek();
        if (c == '{') {
            Object(node, depth);
        } else if (c == '[') {
            Array(node, depth);
        } else if (c == '"') {
            node.kind = TuningJsonKind::String;
            node.text = String();
        } else if (Word("true")) {
            node.kind = TuningJsonKind::Bool;
            node.boolean = true;
        } else if (Word("false")) {
            node.kind = TuningJsonKind::Bool;
        } else if (Word("null")) {
            node.kind = TuningJsonKind::Null;
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            node.kind = TuningJsonKind::Number;
            node.text = Number();
        } else {
            Fail(c ? "expected a JSON value" : "unexpected end of the file");
        }
        node.end = _at;
        return node;
    }

    bool Word(std::string_view word) {
        if (_text.substr(_at, word.size()) != word) return false;
        _at += word.size();
        return true;
    }

    void Object(TuningJsonNode& node, int depth) {
        node.kind = TuningJsonKind::Object;
        ++_at;
        SkipSpace();
        if (Peek() == '}') {
            ++_at;
            return;
        }
        while (true) {
            SkipSpace();
            if (Peek() != '"') Fail("expected a quoted key");
            TuningJsonMember member;
            member.line = _line;
            member.key = String();
            if (node.Find(member.key)) throw TuningFormatError(member.line, "duplicate key \"" + member.key + "\"");
            SkipSpace();
            Expect(':');
            member.value = Value(depth + 1);
            node.members.push_back(std::move(member));
            SkipSpace();
            if (Peek() == ',') {
                ++_at;
                continue;
            }
            Expect('}');
            return;
        }
    }

    void Array(TuningJsonNode& node, int depth) {
        node.kind = TuningJsonKind::Array;
        ++_at;
        SkipSpace();
        if (Peek() == ']') {
            ++_at;
            return;
        }
        while (true) {
            node.items.push_back(Value(depth + 1));
            SkipSpace();
            if (Peek() == ',') {
                ++_at;
                continue;
            }
            Expect(']');
            return;
        }
    }

    [[nodiscard]] bool Digit() const { return Peek() >= '0' && Peek() <= '9'; }

    void Digits() {
        if (!Digit()) Fail("invalid number");
        while (Digit())
            ++_at;
    }

    std::string Number() {
        const std::size_t start = _at;
        if (Peek() == '-') ++_at;
        if (Peek() == '0')
            ++_at;
        else
            Digits();
        if (Peek() == '.') {
            ++_at;
            Digits();
        }
        if (Peek() == 'e' || Peek() == 'E') {
            ++_at;
            if (Peek() == '+' || Peek() == '-') ++_at;
            Digits();
        }
        return std::string(_text.substr(start, _at - start));
    }

    unsigned Hex4() {
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = Peek();
            value <<= 4;
            if (c >= '0' && c <= '9')
                value |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f')
                value |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                value |= static_cast<unsigned>(c - 'A' + 10);
            else
                Fail("invalid escape in a string");
            ++_at;
        }
        return value;
    }

    static void AppendUtf8(std::string& out, unsigned code) {
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    unsigned Escape() {
        const char c = Peek();
        ++_at;
        switch (c) {
        case '"' : return '"';
        case '\\': return '\\';
        case '/' : return '/';
        case 'b' : return '\b';
        case 'f' : return '\f';
        case 'n' : return '\n';
        case 'r' : return '\r';
        case 't' : return '\t';
        case 'u' : break;
        default  : Fail("invalid escape in a string");
        }
        unsigned code = Hex4();
        if (code >= 0xD800 && code <= 0xDBFF) {
            if (!Word("\\u")) Fail("invalid escape in a string");
            const unsigned low = Hex4();
            if (low < 0xDC00 || low > 0xDFFF) Fail("invalid escape in a string");
            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
        } else if (code >= 0xDC00 && code <= 0xDFFF) {
            Fail("invalid escape in a string");
        }
        return code;
    }

    std::string String() {
        ++_at;
        std::string out;
        while (true) {
            const char c = Peek();
            if (c == '\0' || c == '\n') Fail("unterminated string");
            if (c == '"') {
                ++_at;
                return out;
            }
            if (static_cast<unsigned char>(c) < 0x20) Fail("control character in a string");
            ++_at;
            if (c == '\\')
                AppendUtf8(out, Escape());
            else
                out += c;
        }
    }

    std::string_view _text;
    std::size_t _at = 0;
    int _line = 1;
};
} // namespace

const TuningJsonNode* TuningJsonNode::Find(std::string_view key) const {
    for (const auto& member : members)
        if (member.key == key) return &member.value;
    return nullptr;
}

TuningJsonNode ParseTuningJson(std::string_view text) { return Reader(text).Document(); }
