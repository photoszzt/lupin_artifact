use std::env;
use std::path::PathBuf;
use cmake;

fn main() {
    let manifest_dir = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());
    let src = manifest_dir.parent().unwrap();
    let dst = cmake::Config::new(src)
        .define("CMAKE_C_COMPILER", "clang-15")
        .define("CMAKE_CXX_COMPILER", "clang++-15")
        .no_build_target(true)
        .build();
    println!("cargo:rustc-link-search=native={}/build/bin", dst.display());
    println!("cargo:rustc-link-lib=static=shmemcpy");
}
