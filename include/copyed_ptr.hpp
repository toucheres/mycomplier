#pragma once
#include <memory>
#include <utility>

template <class T> class value_ptr
{
    std::unique_ptr<T> ptr;

  public:
    // 静态创建方法
    template <typename... Args> static value_ptr<T> make_copyed_ptr(Args&&... args)
    {
        value_ptr<T> tp;
        tp.ptr = std::make_unique<T>(std::forward<Args>(args)...);
        return tp;
    }

    // 拷贝构造函数
    value_ptr(const value_ptr<T>& other)
        : ptr(other.ptr ? std::make_unique<T>(*other.ptr) : nullptr)
    {
    }

    // 拷贝赋值运算符
    value_ptr& operator=(const value_ptr<T>& other)
    {
        if (this != &other)
        {
            ptr = other.ptr ? std::make_unique<T>(*other.ptr) : nullptr;
        }
        return *this;
    }

    // 移动构造函数
    value_ptr(value_ptr&& other) noexcept = default;

    // 移动赋值运算符
    value_ptr& operator=(value_ptr&& other) noexcept = default;

    // 构造函数，从参数构造T对象
    template <typename... Args>
    explicit value_ptr(Args&&... args) : ptr(std::make_unique<T>(std::forward<Args>(args)...))
    {
    }

    // 默认构造函数
    value_ptr() = default;

    // 析构函数
    ~value_ptr() = default;

    // 从原始指针构造
    explicit value_ptr(T* raw_ptr) : ptr(raw_ptr)
    {
    }

    // 布尔转换运算符，检查指针是否为空
    explicit operator bool() const noexcept
    {
        return static_cast<bool>(ptr);
    }

    // 获取原始指针
    T* get() const noexcept
    {
        return ptr.get();
    }
    // ==运算符
    bool operator==(std::nullptr_t) const
    {
        return ptr == nullptr;
    }
    // 解引用运算符
    T& operator*() const
    {
        return *ptr;
    }

    // 成员访问运算符
    T* operator->() const noexcept
    {
        return ptr.get();
    }

    // 重置指针
    void reset(T* p = nullptr) noexcept
    {
        ptr.reset(p);
    }

    // 释放指针所有权
    T* release() noexcept
    {
        return ptr.release();
    }

    // 交换两个指针
    void swap(value_ptr& other) noexcept
    {
        ptr.swap(other.ptr);
    }
};