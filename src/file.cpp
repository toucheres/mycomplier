#include "file.hpp"
#include <algorithm>
#include <fstream>
file::file(const std::string& content_str, bool from_str) : path("")
{
    if (from_str)
        content = std::vector<char>(content_str.begin(), content_str.end());
    pos = 0;
}
file::file(const std::string& path) : path(path)
{
    readform(path);
}
const std::string& file::getpath()
{
    return this->path;
}
bool file::writeto(const std::string& path)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;
    out.write(content.data(), content.size());
    return out.good();
}

bool file::readform(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    content =
        std::vector<char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    pos = 0;
    this->path = path;
    return true;
}

size_t file::size() const
{
    return content.size();
}

file::file(const file& copy) : content(copy.content), pos(copy.pos)
{
}

file::file(file&& move) : content(std::move(move.content)), pos(move.pos)
{
}

bool file::setpos(size_t where)
{
    if (where < size())
    {
        pos = where;
        return true;
    }
    return false;
}

size_t file::getpos()
{
    return pos;
}

bool file::readline(std::string& out) const
{
    if (pos >= content.size())
        return false;
    out.clear();
    size_t i = pos;
    while (i < content.size())
    {
        char c = content[i];
        if (c == '\n')
        {
            ++i;
            break;
        }
        out += c;
        ++i;
    }
    if (i == pos)
        return false;
    pos = i;
    return true;
}

bool file::readalllast(std::string& out) const
{
    return this->readnum(out, this->size() - pos);
}

bool file::readnum(std::string& out, size_t num) const
{
    if (pos >= content.size())
        return false;
    out.clear();
    size_t remain = content.size() - pos;
    size_t n = std::min(num, remain);
    out.assign(content.begin() + pos, content.begin() + pos + n);
    pos += n;
    return n > 0;
}

bool file::readuntil(const std::string& what, std::string& out) const
{
    if (pos >= content.size())
        return false;
    out.clear();
    size_t i = pos;
    while (i + what.size() <= content.size())
    {
        if (std::equal(what.begin(), what.end(), content.begin() + i))
        {
            break;
        }
        out += content[i];
        ++i;
    }
    pos = i;
    return !out.empty();
}

bool file::unread(size_t num) const
{
    if (num > pos)
        return false;
    pos -= num;
    return true;
}

bool file::insert(const std::string& in, size_t where)
{
    if (where > content.size())
        return false;
    content.insert(content.begin() + where, in.begin(), in.end());
    return true;
}

bool file::insert(const std::string& in)
{
    return insert(in, pos);
}