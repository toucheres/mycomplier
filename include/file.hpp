#pragma once
#include <string>
#include <vector>
class file
{
    std::vector<char> content;
    mutable size_t pos = 0;
    std::string path;

  public:
    bool writeto(const std::string& path);
    bool readform(const std::string& path);
    const std::string& getpath();
    explicit file(const std::string& path);
    explicit file(const std::string& content_str, bool from_str);
    size_t size() const;
    file(const file& copy);
    file(file&& move);
    bool setpos(size_t where);
    size_t getpos();
    bool readline(std::string& out) const;
    bool readalllast(std::string& out) const;
    bool readnum(std::string& out, size_t num) const;
    bool readuntil(const std::string& what, std::string& out) const;
    bool unread(size_t num) const;
    bool insert(const std::string& in, size_t where);
    bool insert(const std::string& in);
};