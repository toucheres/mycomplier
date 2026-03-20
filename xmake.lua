-- xmake.lua - 从 CMakeLists.txt 翻译而来，作为起点。请根据本机环境调整依赖安装路径。

set_project("mycomplier")
set_version("0.1")
set_languages("c++23")

add_rules("mode.debug", "mode.release")
add_requires("boost", {configs = {program_options = true, cmake = false}})
add_requires("magic_enum","antlr4 4.13.2","antlr4-runtime 4.13.2")
-- 在 debug 或 relwithdebinfo 模式下为 GNU 编译器添加 -pg（gprof）支持
if is_mode("debug") or is_mode("relwithdebinfo") then
    add_cxxflags("-pg", {force = true})
    add_ldflags("-pg", {force = true})
end

-- package("antlr4.13.2_runtime")
--     add_deps("cmake")
--     set_sourcedir(path.join(os.scriptdir(), "antlr4.13.2"))
--     on_install(function (package)
--         local configs = {}
--         table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
--         table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
--         -- 跳过 C++ 测试，避免 FetchContent 联网拉取 googletest
--         table.insert(configs, "-DANTLR_BUILD_CPP_TESTS=OFF")
--         -- 只构建运行时库，不构建额外示例/测试目标
--         table.insert(configs, "-DANTLR_BUILD_SHARED=ON")
--         table.insert(configs, "-DANTLR_BUILD_STATIC=OFF")
--         import("package.tools.cmake").install(package, configs)
--     end)
-- package_end()

-- add_requires("antlr4.13.2_runtime")

-- 主目标
target("mycomplier")
    set_kind("binary")

    -- 源文件（包含 src、grammar 支撑代码和生成目录）
    add_files("src/**.cpp")
    add_files("grammar/Cpp/*.cpp")
    add_files("generated/**.cpp")

    -- 头文件搜索路径
    add_includedirs("include","grammar/Cpp","generated")

    -- 链接库
    add_packages("magic_enum","antlr4","antlr4-runtime","boost")

    -- 在构建前运行 ANTLR 生成 C++ 源文件
    before_build(function (target)
        os.exec("mkdir -p generated")
        local cmd =  "java -jar ./antlr4.13.2/antlr-4.13.2-complete.jar -Dlanguage=Cpp -visitor -listener -o generated -Xexact-output-dir grammar/CLexer.g4 grammar/CParser.g4"
        print("run: " .. cmd)
        os.exec(cmd)
    end)


    -- 设置为使用 c++23 标准（已在 project 级别设置，但可在 target 级别加强）
    set_languages("c++23")
    add_defines("PROJECT_NAME=\"mycomplier\"")
    add_rules("mode.debug", "mode.release")
target_end()

-- 测试目录：如果需要可在此处添加 test target 或启用子目录
-- 可以在此文件中添加额外 targets 来替代 CMake 的 add_subdirectory(test)

-- 简短使用说明：
-- 1) 若需要 xmake 管理依赖：`xmake require --save boost` （可选 magic_enum）
-- 2) 生成并构建：`xmake` 或显式 debug 模式 `xmake f -m debug && xmake`
-- 3) 如果 antlr 未在 PATH，请先安装或把 antlr4 可执行加入 PATH。
