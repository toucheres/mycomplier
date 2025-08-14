#pragma once
#include <string>
#include <vector>
class file
{
    std::vector<char> content;
    mutable int pos = 0;
    std::string path;

  public:
    bool writeto(const std::string& path);
    bool readform(const std::string& path);
    const std::string& getpath();
    explicit file(const std::string& path);
    explicit file(const std::string& content_str, bool from_str);
    int size() const;
    file(const file& copy);
    file(file&& move);
    bool setpos(int where);
    int getpos();
    bool readline(std::string& out) const;
    bool readalllast(std::string& out) const;
    bool readnum(std::string& out, int num) const;
    bool readuntil(const std::string& what, std::string& out) const;
    bool unread(int num) const;
    bool insert(const std::string& in, int where);
    bool insert(const std::string& in);
};