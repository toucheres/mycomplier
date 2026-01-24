#pragma once
#ifndef TREE_SCOPED_MAP_HPP
#define TREE_SCOPED_MAP_HPP

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

// tree_scoped_map mimics scoped_map but keeps popped scopes in a tree so they
// remain inspectable (lookups only see the active branch).
// Label must be equality comparable when used, ScopeInfo lets callers attach
// per-scope metadata and can be inspected via for_each_scope.
template <class KT, class VT, class Label = std::monostate, class ScopeInfo = std::monostate>
class tree_scoped_map
{
  private:
    using scope_type = std::map<KT, VT>;

    struct node
    {
        scope_type entries;
        ScopeInfo info{};
        std::optional<Label> label;
        node* parent = nullptr;
        std::vector<std::unique_ptr<node>> children;

        node(node* p, std::optional<Label> lbl, ScopeInfo data)
            : info(std::move(data)), label(std::move(lbl)), parent(p)
        {
        }
    };

  public:
    tree_scoped_map() : root_(std::make_unique<node>(nullptr, std::nullopt, ScopeInfo{}))
    {
        current_ = root_.get();
    }

    tree_scoped_map(const tree_scoped_map&) = delete;
    tree_scoped_map& operator=(const tree_scoped_map&) = delete;

    tree_scoped_map(tree_scoped_map&&) noexcept = default;
    tree_scoped_map& operator=(tree_scoped_map&&) noexcept = default;

    void clear()
    {
        root_ = std::make_unique<node>(nullptr, std::nullopt, ScopeInfo{});
        current_ = root_.get();
    }

    // Enter a child scope without label or metadata.
    void enter_scope()
    {
        push_scope(std::nullopt, ScopeInfo{});
    }

    // Enter a child scope with label.
    void enter_scope(const Label& lbl)
    {
        push_scope(lbl, ScopeInfo{});
    }

    void enter_scope(Label&& lbl)
    {
        push_scope(std::move(lbl), ScopeInfo{});
    }

    // Enter with label and metadata.
    void enter_scope(const Label& lbl, ScopeInfo info)
    {
        push_scope(lbl, std::move(info));
    }

    void enter_scope(Label&& lbl, ScopeInfo info)
    {
        push_scope(std::move(lbl), std::move(info));
    }

    // Enter with metadata only.
    void enter_scope_with_info(ScopeInfo info)
    {
        push_scope(std::nullopt, std::move(info));
    }

    // Compatibility with scoped_map naming.
    void in_scope()
    {
        enter_scope();
    }
    void in_scope(const Label& lbl)
    {
        enter_scope(lbl);
    }
    void in_scope(Label&& lbl)
    {
        enter_scope(std::move(lbl));
    }
    void in_scope(const Label& lbl, ScopeInfo info)
    {
        enter_scope(lbl, std::move(info));
    }
    void in_scope(Label&& lbl, ScopeInfo info)
    {
        enter_scope(std::move(lbl), std::move(info));
    }
    void in_scope_with_info(ScopeInfo info)
    {
        enter_scope_with_info(std::move(info));
    }

    // Move to parent scope; the popped scope stays in the tree but is no
    // longer visible for lookups.
    scope_type& out_scope()
    {
        if (!current_->parent)
        {
            throw std::runtime_error("cannot pop the root scope");
        }

        node* popped = current_;
        current_ = current_->parent;
        return popped->entries;
    }

    bool add(const KT& k, const VT& v)
    {
        auto& cur = current_->entries;
        auto [_, inserted] = cur.emplace(k, v);
        return inserted;
    }

    VT& operator[](const KT& k)
    {
        for (node* it = current_; it != nullptr; it = it->parent)
        {
            auto pos = it->entries.find(k);
            if (pos != it->entries.end())
            {
                return pos->second;
            }
        }
        return current_->entries[k];
    }

    bool contains(const KT& k) const
    {
        return find(k) != nullptr;
    }

    std::size_t scope_depth() const
    {
        std::size_t depth = 0;
        for (node* it = current_; it != nullptr; it = it->parent)
        {
            ++depth;
        }
        return depth;
    }

    std::size_t current_scope_size() const
    {
        return current_->entries.size();
    }

    std::size_t total_size() const
    {
        return total_size_from(root_.get());
    }

    scope_type& current_scope()
    {
        return current_->entries;
    }

    const scope_type& current_scope() const
    {
        return current_->entries;
    }

    ScopeInfo& current_scope_info()
    {
        return current_->info;
    }

    const ScopeInfo& current_scope_info() const
    {
        return current_->info;
    }

    const std::optional<Label>& current_scope_label() const
    {
        return current_->label;
    }

    VT* find(const KT& k)
    {
        for (node* it = current_; it != nullptr; it = it->parent)
        {
            auto pos = it->entries.find(k);
            if (pos != it->entries.end())
            {
                return &pos->second;
            }
        }
        return nullptr;
    }

    const VT* find(const KT& k) const
    {
        for (const node* it = current_; it != nullptr; it = it->parent)
        {
            auto pos = it->entries.find(k);
            if (pos != it->entries.end())
            {
                return &pos->second;
            }
        }
        return nullptr;
    }

    // Collect all scope maps at the given depth (root depth is 0).
    std::vector<scope_type*> getwheredeeps(std::size_t depth)
    {
        std::vector<scope_type*> scopes;
        collect_at_depth(root_.get(), 0, depth, scopes);
        return scopes;
    }

    std::vector<const scope_type*> getwheredeeps(std::size_t depth) const
    {
        std::vector<const scope_type*> scopes;
        collect_at_depth(root_.get(), 0, depth, scopes);
        return scopes;
    }

    // Visit every scope node in DFS order. Depth is zero-based from the root.
    template <class Fn> void for_each_scope(Fn&& fn)
    {
        for_each_scope_impl(root_.get(), 0, std::forward<Fn>(fn));
    }

    template <class Fn> void for_each_scope(Fn&& fn) const
    {
        for_each_scope_impl(root_.get(), 0, std::forward<Fn>(fn));
    }

    // Find scopes by label anywhere in the tree.
    std::vector<scope_type*> find_scopes_by_label(const Label& lbl)
    {
        std::vector<scope_type*> scopes;
        collect_by_label(root_.get(), lbl, scopes);
        return scopes;
    }

    std::vector<const scope_type*> find_scopes_by_label(const Label& lbl) const
    {
        std::vector<const scope_type*> scopes;
        collect_by_label(root_.get(), lbl, scopes);
        return scopes;
    }

  private:
    node* push_scope(std::optional<Label> lbl, ScopeInfo info)
    {
        current_->children.push_back(
            std::make_unique<node>(current_, std::move(lbl), std::move(info)));
        current_ = current_->children.back().get();
        return current_;
    }

    std::size_t total_size_from(const node* n) const
    {
        std::size_t n_items = n->entries.size();
        for (const auto& child : n->children)
        {
            n_items += total_size_from(child.get());
        }
        return n_items;
    }

    template <class Fn>
    void for_each_scope_impl(node* n, std::size_t depth, Fn&& fn)
    {
        fn(depth, n->entries, n->info, n->label);
        for (auto& child : n->children)
        {
            for_each_scope_impl(child.get(), depth + 1, fn);
        }
    }

    template <class Fn>
    void for_each_scope_impl(const node* n, std::size_t depth, Fn&& fn) const
    {
        fn(depth, n->entries, n->info, n->label);
        for (const auto& child : n->children)
        {
            for_each_scope_impl(child.get(), depth + 1, fn);
        }
    }

    template <class PtrVec>
    void collect_at_depth(node* n, std::size_t cur_depth, std::size_t target, PtrVec& out) const
    {
        if (cur_depth == target)
        {
            out.push_back(&n->entries);
            return;
        }

        for (auto& child : n->children)
        {
            collect_at_depth(child.get(), cur_depth + 1, target, out);
        }
    }

    void collect_by_label(node* n, const Label& lbl, std::vector<scope_type*>& out)
    {
        if (n->label && *n->label == lbl)
        {
            out.push_back(&n->entries);
        }

        for (auto& child : n->children)
        {
            collect_by_label(child.get(), lbl, out);
        }
    }

    void collect_by_label(const node* n, const Label& lbl,
                          std::vector<const scope_type*>& out) const
    {
        if (n->label && *n->label == lbl)
        {
            out.push_back(&n->entries);
        }

        for (const auto& child : n->children)
        {
            collect_by_label(child.get(), lbl, out);
        }
    }

  private:
    std::unique_ptr<node> root_;
    node* current_ = nullptr;
};

#endif // TREE_SCOPED_MAP_HPP
