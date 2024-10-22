"""A macro providing an end-to-end library for jaxite codegen."""

load("@heir//tools:heir-opt.bzl", "heir_opt")
load("@heir//tools:heir-translate.bzl", "heir_translate")
load("@rules_python//python:py_library.bzl", "py_library")

def fhe_jaxite_lib(name, mlir_src, entry_function_flag = "", py_lib_target_name = "", tags = [], deps = [], **kwargs):
    """A rule for generating Jaxite code.

    Args:
      name: The name of the py_test target and the generated .cc file basename.
      mlir_src: The source mlir file to run through heir-translate.
      entry_function_flag: Flags for entry function.
      py_lib_target_name: target_name for the py_library.
      tags: Tags to pass to py_test.
      deps: Deps to pass to py_test and py_library.
      **kwargs: Keyword arguments to pass to py_library and py_test.
    """
    heir_opt_name = name + ".heir_opt"
    generated_heir_opt_name = "%s.heir_opt.mlir" % name
    py_codegen_target = name + ".heir_translate_py"
    generated_py_filename = "%s_lib.py" % name
    if not py_lib_target_name:
        py_lib_target_name = "%s_py_lib" % name

    if entry_function_flag:
        heir_opt_pass_flag = "--tosa-to-boolean-jaxite=%s" % entry_function_flag
        heir_opt(
            name = heir_opt_name,
            src = mlir_src,
            pass_flag = heir_opt_pass_flag,
            generated_filename = generated_heir_opt_name,
            HEIR_YOSYS = True,
            data = ["@heir//lib/Transforms/YosysOptimizer/yosys:share_files"],
        )
    else:
        generated_heir_opt_name = mlir_src

    heir_translate(
        name = py_codegen_target,
        src = generated_heir_opt_name,
        pass_flags = ["--emit-jaxite"],
        generated_filename = generated_py_filename,
    )
    py_library(
        name = py_lib_target_name,
        srcs = [":" + generated_py_filename],
        deps = deps + ["@heir_pip_deps_jaxite//:pkg"],
        tags = tags,
        **kwargs
    )
