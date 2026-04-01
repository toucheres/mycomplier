#pragma once
#include <string>
inline long align_up(long num, long align)
{
    if (num % align == 0)
    {
        return num;
    }
    else
    {
        return num + (align - num % align);
    }
};
// Shared helpers for decoding C string/character literals (handles prefixes, raw strings,
// common escape sequences, \x hex and octal escapes). Returns false on malformed input.

inline bool decode_string_token_text(const std::string& token, size_t& p, std::string& out)
{
    // p is input/output index into token; on success p advances to char after literal
    if (p >= token.size())
        return false;
    // parse optional encoding prefix: u8 | u | U | L
    size_t q = p;
    if (q + 1 < token.size() && token[q] == 'u' && token[q + 1] == '8')
        q += 2;
    else if (token[q] == 'u' || token[q] == 'U' || token[q] == 'L')
        q += 1;

    // raw string: R"delim(... )delim"
    if (q + 1 < token.size() && token[q] == 'R' && token[q + 1] == '"')
    {
        size_t delim_start = q + 2;
        size_t delim_end = token.find('(', delim_start);
        if (delim_end == std::string::npos)
            return false;
        std::string delim = token.substr(delim_start, delim_end - delim_start);
        size_t content_start = delim_end + 1;
        std::string close = std::string(")") + delim + '"';
        size_t close_pos = token.find(close, content_start);
        if (close_pos == std::string::npos)
            return false;
        out.append(token, content_start, close_pos - content_start);
        p = close_pos + close.size();
        return true;
    }

    // normal string literal: "..."
    if (q < token.size() && token[q] == '"')
    {
        size_t i = q + 1;
        while (i < token.size())
        {
            char c = token[i];
            if (c == '"')
            {
                p = i + 1;
                return true;
            }
            if (c == '\\' && i + 1 < token.size())
            {
                char esc = token[i + 1];
                switch (esc)
                {
                case 'n':
                    out.push_back('\n');
                    i += 2;
                    break;
                case 't':
                    out.push_back('\t');
                    i += 2;
                    break;
                case 'r':
                    out.push_back('\r');
                    i += 2;
                    break;
                case '\\':
                    out.push_back('\\');
                    i += 2;
                    break;
                case '\'':
                    out.push_back('\'');
                    i += 2;
                    break;
                case '"':
                    out.push_back('"');
                    i += 2;
                    break;
                case 'a':
                    out.push_back('\a');
                    i += 2;
                    break;
                case 'b':
                    out.push_back('\b');
                    i += 2;
                    break;
                case 'f':
                    out.push_back('\f');
                    i += 2;
                    break;
                case 'v':
                    out.push_back('\v');
                    i += 2;
                    break;
                case '?':
                    out.push_back('?');
                    i += 2;
                    break;
                case 'x':
                {
                    i += 2;
                    int val = 0;
                    bool any = false;
                    while (i < token.size() && std::isxdigit((unsigned char)token[i]))
                    {
                        char ch = token[i];
                        int v = (ch >= '0' && ch <= '9')   ? ch - '0'
                                : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10
                                                           : ch - 'A' + 10;
                        val = val * 16 + v;
                        i++;
                        any = true;
                    }
                    if (any)
                        out.push_back(static_cast<char>(val));
                    break;
                }
                default:
                    if (esc >= '0' && esc <= '7')
                    {
                        int val = esc - '0';
                        i += 2;
                        int cnt = 1;
                        while (cnt < 3 && i < token.size() && token[i] >= '0' && token[i] <= '7')
                        {
                            val = val * 8 + (token[i] - '0');
                            i++;
                            cnt++;
                        }
                        out.push_back(static_cast<char>(val));
                    }
                    else
                    {
                        out.push_back(esc);
                        i += 2;
                    }
                }
            }
            else
            {
                out.push_back(c);
                i++;
            }
        }
    }
    return false;
}

inline bool decode_character_token(const std::string& token, int& outval)
{
    // token expected like 'a' or '\n' or '\x41' or '\123'
    if (token.size() < 2)
        return false;
    if (token.front() != '\'' || token.back() != '\'')
        return false;
    std::string content = token.substr(1, token.size() - 2);
    if (content.empty())
        return false;
    if (content[0] != '\\')
    {
        outval = static_cast<unsigned char>(content[0]);
        return true;
    }
    // escape
    if (content.size() < 2)
        return false;
    char esc = content[1];
    switch (esc)
    {
    case 'n':
        outval = '\n';
        return true;
    case 't':
        outval = '\t';
        return true;
    case 'r':
        outval = '\r';
        return true;
    case '0':
        outval = '\0';
        return true;
    case '\\':
        outval = '\\';
        return true;
    case '\'':
        outval = '\'';
        return true;
    case '"':
        outval = '"';
        return true;
    case 'x':
    {
        if (content.size() < 3)
            return false;
        int val = 0;
        for (size_t i = 2; i < content.size(); ++i)
        {
            char ch = content[i];
            if (!std::isxdigit((unsigned char)ch))
                break;
            int v = (ch >= '0' && ch <= '9')   ? ch - '0'
                    : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10
                                               : ch - 'A' + 10;
            val = val * 16 + v;
        }
        outval = val;
        return true;
    }
    default:
        if (esc >= '0' && esc <= '7')
        {
            int val = esc - '0';
            size_t i = 2;
            int cnt = 1;
            while (cnt < 3 && i < content.size() && content[i] >= '0' && content[i] <= '7')
            {
                val = val * 8 + (content[i] - '0');
                i++;
                cnt++;
            }
            outval = val;
            return true;
        }
        outval = static_cast<unsigned char>(esc);
        return true;
    }
}