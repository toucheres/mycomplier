#pragma once
#include <boost/program_options.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace typed_options {

// 选项值的各种配置
template<typename T>
struct OptionConfig {
    std::optional<T> default_value;
    std::optional<T> implicit_value;  // 用于 bool 类型等，不带参数时的值
};

// 完整的选项定义（编译期类型安全）
template<typename T>
struct Option {
    using value_type = T;
    
    std::string_view name;           // 主名称（可包含逗号分隔的别名）
    std::string_view description;     // 描述文本
    OptionConfig<T> config;          // 默认值和隐式值配置
    
    constexpr Option(std::string_view n, std::string_view desc = "", 
                    std::optional<T> def_val = std::nullopt,
                    std::optional<T> impl_val = std::nullopt)
        : name(n), description(desc), config{def_val, impl_val} {}
    
    constexpr const char* c_str() const { return name.data(); }
    constexpr operator std::string_view() const { return name; }
    
    // 提取主名称（逗号前的部分）
    std::string primary_name() const {
        auto pos = name.find(',');
        if (pos != std::string_view::npos) {
            return std::string(name.substr(0, pos));
        }
        return std::string(name);
    }
    
    // 提取所有别名
    std::vector<std::string> all_names() const {
        std::vector<std::string> names;
        std::string_view remaining = name;
        size_t pos;
        while ((pos = remaining.find(',')) != std::string_view::npos) {
            auto part = remaining.substr(0, pos);
            // 去除空格
            while (!part.empty() && part[0] == ' ') part.remove_prefix(1);
            while (!part.empty() && part.back() == ' ') part.remove_suffix(1);
            if (!part.empty()) {
                names.push_back(std::string(part));
            }
            remaining = remaining.substr(pos + 1);
        }
        // 处理最后一个
        while (!remaining.empty() && remaining[0] == ' ') remaining.remove_prefix(1);
        while (!remaining.empty() && remaining.back() == ' ') remaining.remove_suffix(1);
        if (!remaining.empty()) {
            names.push_back(std::string(remaining));
        }
        return names;
    }
};

// 前向声明
class TypedVariablesMap;

// 链式调用的中间对象
template<typename T>
class OptionGetter {
private:
    const TypedVariablesMap* vm_;
    const Option<T>* option_;
    std::optional<T> value_;
    
    friend class TypedVariablesMap;
    
    OptionGetter(const TypedVariablesMap* vm, const Option<T>* opt, std::optional<T> val)
        : vm_(vm), option_(opt), value_(val) {}

public:
    // 提供默认值（如果选项不存在）
    T or_default(T default_value) const {
        return value_.value_or(default_value);
    }
    
    // 简写形式
    T or_(T default_value) const {
        return or_default(default_value);
    }
    
    // 尝试下一个选项
    OptionGetter<T> or_try(const Option<T>& next_option) const;
    
    // 转换为可选值
    std::optional<T> optional() const {
        return value_;
    }
    
    // 隐式转换为值（如果存在）
    operator T() const {
        if (value_) {
            return *value_;
        }
        throw std::runtime_error("Option value not found");
    }
    
    // 检查是否有值
    bool has_value() const {
        return value_.has_value();
    }
};

// 类型安全的 variables_map 包装器
class TypedVariablesMap {
private:
    const boost::program_options::variables_map& vm_;

public:
    explicit TypedVariablesMap(const boost::program_options::variables_map& vm) : vm_(vm) {}

    // 检查选项是否存在（支持多个别名）
    template<typename T>
    bool has(const Option<T>& opt) const {
        for (const auto& name : opt.all_names()) {
            if (vm_.count(name) > 0) {
                return true;
            }
        }
        return false;
    }

    // 获取选项值（返回链式调用对象）
    template<typename T>
    OptionGetter<T> get(const Option<T>& opt) const {
        std::optional<T> value;
        for (const auto& name : opt.all_names()) {
            if (vm_.count(name) > 0) {
                value = vm_[name].template as<T>();
                break;
            }
        }
        return OptionGetter<T>(this, &opt, value);
    }
    
    // 直接获取值（无链式调用）
    template<typename T>
    T get_direct(const Option<T>& opt) const {
        for (const auto& name : opt.all_names()) {
            if (vm_.count(name) > 0) {
                return vm_[name].template as<T>();
            }
        }
        throw std::runtime_error("Option not found: " + opt.primary_name());
    }
    
    // 获取原始的 variables_map
    const boost::program_options::variables_map& raw() const {
        return vm_;
    }
};

// 实现 OptionGetter 的 or_try 方法
template<typename T>
OptionGetter<T> OptionGetter<T>::or_try(const Option<T>& next_option) const {
    if (value_) {
        return *this;
    }
    return vm_->get(next_option);
}

// 类型特征：检测是否为 vector 类型
template<typename T>
struct is_vector : std::false_type {};

template<typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};

template<typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

// 辅助函数：注册选项到 boost::program_options
template<typename T>
void add_option(boost::program_options::options_description& desc, const Option<T>& opt) {
    auto po_opt = boost::program_options::value<T>();
    
    // vector 类型不支持 default_value 和 implicit_value（因为无法 lexical_cast）
    if constexpr (!is_vector_v<T>) {
        if (opt.config.default_value) {
            po_opt->default_value(*opt.config.default_value);
        }
        
        if (opt.config.implicit_value) {
            po_opt->implicit_value(*opt.config.implicit_value);
        }
    }
    
    desc.add_options()
        (std::string(opt.name).c_str(), po_opt, std::string(opt.description).c_str());
}

// 特化：bool 类型自动添加隐式值 true
template<>
inline void add_option<bool>(boost::program_options::options_description& desc, const Option<bool>& opt) {
    auto po_opt = boost::program_options::value<bool>();
    
    if (opt.config.default_value) {
        po_opt->default_value(*opt.config.default_value);
    }
    
    // bool 类型默认隐式值为 true（不带参数时的值）
    if (opt.config.implicit_value) {
        po_opt->implicit_value(*opt.config.implicit_value);
    } else {
        po_opt->implicit_value(true);
    }
    
    desc.add_options()
        (std::string(opt.name).c_str(), po_opt, std::string(opt.description).c_str());
}

} // namespace typed_options
