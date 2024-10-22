"""A macro providing an end-to-end test for jaxite codegen."""

load("@heir//tools:heir-jaxite.bzl", "fhe_jaxite_lib")
load("@rules_python//python:py_test.bzl", "py_test")

def jaxite_end_to_end_test(name, mlir_src, test_src, entry_function_flag = "", tags = [], deps = [], **kwargs):
    py_lib_target_name = "%s_py_lib" % name
    fhe_jaxite_lib(name, mlir_src, entry_function_flag, py_lib_target_name, tags, deps, **kwargs)
    py_test(
        name = name,
        srcs = [test_src],
        main = test_src,
        deps = deps + [
            ":" + py_lib_target_name,
            "@heir_pip_deps_jaxite//:pkg",
            "@com_google_absl_py//absl/testing:absltest",
        ],
    )
