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

    -- 在构建前运行 ANTLR 生成 C++ 源文件（使用 xmake 管理的 antlr4 包，确保跨平台）
    before_build(function (target)
        import("lib.detect.find_program")
        
        os.mkdir("generated")
        
        -- 获取 xmake 安装的 antlr4 包路径
        local antlr4_pkg = target:pkg("antlr4")
        if not antlr4_pkg then
            raise("antlr4 package not found, please run 'xmake f -c' to reconfigure")
        end
        
        local antlr4_installdir = antlr4_pkg:installdir()
        local antlr_jar = path.join(antlr4_installdir, "lib", "antlr-complete.jar")
        
        if not os.isfile(antlr_jar) then
            raise("antlr-complete.jar not found at: " .. antlr_jar)
        end
        
        -- 检测 java 命令（antlr4 需要 JRE/JDK）
        local java = find_program("java")
        if not java then
            raise("java not found, please install JRE/JDK to run ANTLR")
        end
        
        -- 检查是否需要重新生成（grammar 文件修改时间晚于生成文件）
        local grammar_files = {"grammar/CLexer.g4", "grammar/CParser.g4"}
        local generated_files = {"generated/CLexer.cpp", "generated/CParser.cpp"}
        local need_regenerate = false
        
        for _, gfile in ipairs(grammar_files) do
            for _, genfile in ipairs(generated_files) do
                if not os.isfile(genfile) or os.mtime(gfile) > os.mtime(genfile) then
                    need_regenerate = true
                    break
                end
            end
            if need_regenerate then break end
        end
        
        if need_regenerate then
            print("Running ANTLR code generation from: " .. antlr_jar)
            os.execv(java, {"-jar", antlr_jar, "-Dlanguage=Cpp", "-visitor", "-listener", 
                           "-o", "generated", "-Xexact-output-dir", 
                           "grammar/CLexer.g4", "grammar/CParser.g4"})
            print("ANTLR code generation completed")
        else
            print("ANTLR generated files are up to date, skipping generation")
        end
    end)

    after_build(function (target)
        local  src_dir = "stdlibc"
        local dst_dir = path.join(target:targetdir(), path.filename(src_dir))
        os.cp(src_dir,dst_dir)
        src_dir = "stdhead" 
        dst_dir = path.join(target:targetdir(), path.filename(src_dir))
        os.cp(src_dir,dst_dir)
    end)

    -- 设置为使用 c++23 标准（已在 project 级别设置，但可在 target 级别加强）
    set_languages("c++23")
    add_defines("PROJECT_NAME=\"mycomplier\"")
    add_rules("mode.debug", "mode.release")
target_end()

-- 测试目录：如果需要可在此处添加 test target 或启用子目录
-- 可以在此文件中添加额外 targets 来替代 CMake 的 add_subdirectory(test)

-- 使用说明（开箱即用，跨平台）：
-- 1) 首次配置：`xmake f -c` (自动下载并安装所有依赖，包括 antlr4、boost 等)
-- 2) 构建项目：`xmake` (自动运行 ANTLR 生成代码并编译)
-- 3) 清理构建：`xmake clean` (保留生成的代码) 或 `xmake clean -a` (清除所有)
-- 4) 切换模式：`xmake f -m debug && xmake` (debug 模式) 或 `xmake f -m release && xmake`
-- 
-- 依赖要求：
-- - JRE/JDK (用于运行 ANTLR jar 生成 C++ 代码)
-- - xmake 会自动管理其他所有依赖，无需手动安装
--
-- ANTLR 代码生成：
-- - 使用 xmake 管理的 antlr4 包，不依赖系统安装
-- - 当 grammar/*.g4 文件修改时自动重新生成
-- - 生成的文件位于 generated/ 目录
