#pragma once
#ifndef SCOPED_MAP_HPP
#define SCOPED_MAP_HPP

#include <map>
#include <stdexcept>
#include <vector>

// scoped_map mimics C-like block scoping: keys must be unique per scope,
// outer scopes may reuse names, and lookups search from inner to outer.
template <class KT, class VT> class scoped_map
{
  private:
    using scope_type = std::map<KT, VT>;
    using stack_type = std::vector<scope_type>;

  public:
    scoped_map() { scopes_.emplace_back(); }
    scoped_map(const scoped_map&) = default;
    scoped_map(scoped_map&&) noexcept = default;
    scoped_map& operator=(const scoped_map&) = default;
    scoped_map& operator=(scoped_map&&) noexcept = default;

    void clear()
    {
        scopes_.clear();
        scopes_.emplace_back(); // rebuild root scope
    }

    void in_scope()
    {
        scopes_.emplace_back();
    }

    scope_type out_scope()
    {
        if (scopes_.size() <= 1)
        {
            throw std::runtime_error("cannot pop the root scope");
        }

        auto popped = std::move(scopes_.back());
        scopes_.pop_back();
        return popped;
    }

    bool add(const KT& k, const VT& v)
    {
        auto& cur = scopes_.back();
        auto [_, inserted] = cur.emplace(k, v);
        return inserted; // false when the key already exists in this scope
    }

    VT& operator[](const KT& k)
    {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
        {
            auto pos = it->find(k);
            if (pos != it->end())
            {
                return pos->second;
            }
        }
        // If not found, create in the current (innermost) scope.
        return scopes_.back()[k];
    }

    bool contains(const KT& k) const { return find(k) != nullptr; }

    std::size_t scope_depth() const { return scopes_.size(); }

    std::size_t current_scope_size() const { return scopes_.back().size(); }

    std::size_t total_size() const
    {
        std::size_t n = 0;
        for (const auto& s : scopes_)
        {
            n += s.size();
        }
        return n;
    }

    scope_type& current_scope() { return scopes_.back(); }

    const scope_type& current_scope() const { return scopes_.back(); }

    VT* find(const KT& k)
    {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
        {
            auto pos = it->find(k);
            if (pos != it->end())
            {
                return &pos->second;
            }
        }
        return nullptr;
    }

    const VT* find(const KT& k) const
    {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
        {
            auto pos = it->find(k);
            if (pos != it->end())
            {
                return &pos->second;
            }
        }
        return nullptr;
    }

  private:
    stack_type scopes_;
};

#endif // SCOPED_MAP_HPP