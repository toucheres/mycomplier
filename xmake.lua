-- xmake.lua - xmake build for mycomplier.
--
-- The xmake build uses the ANTLR 4.13.2 runtime package and the vendored
-- antlr-4.13.2 jar.  The CMake build can use the system ANTLR installation;
-- both generate a matching parser/runtime pair for their own toolchain.

set_project("mycomplier")
set_version("0.1")
set_languages("c++23")

add_rules("mode.debug", "mode.release")
add_requires("boost", "antlr4-runtime 4.13.2")

if is_mode("debug") or is_mode("relwithdebinfo") then
    add_cxxflags("-pg", {force = true})
    add_ldflags("-pg", {force = true})
end

target("mycomplier")
    set_kind("binary")

    add_files("src/**.cpp")
    add_files("grammar/Cpp/*.cpp")
    add_files("generated/**.cpp")

    add_includedirs("include", "grammar/Cpp", "generated", "third_party/magic_enum/include")
    add_packages("antlr4-runtime", "boost")

    add_defines(
        "PROJECT_NAME=\"mycomplier\"",
        'MYCOMPLIER_ROOT="' .. os.projectdir() .. '"'
    )

    -- Regenerate the checked-in generated/ directory before compiling.  The
    -- generated sources are also in the repository, so a checkout can be built
    -- without running ANTLR first; this hook keeps them in sync with the
    -- grammar when it changes.
    before_build(function (target)
        os.cd(os.projectdir())
        local generated = "generated"
        local jar = "antlr4.13.2/antlr-4.13.2-complete.jar"
        os.mkdir(generated)

        local args = {
            "-Dlanguage=Cpp",
            "-visitor",
            "-listener",
            "-Xexact-output-dir",
            "-o", generated,
            "grammar/CLexer.g4",
            "grammar/CParser.g4",
        }

        if os.isfile(jar) then
            os.execv("java", table.join({"-jar", jar}, args))
        else
            -- Fallback: use antlr4 from PATH (its version must match the
            -- antlr4-runtime package used above).
            os.execv("antlr4", args)
        end
    end)
target_end()
