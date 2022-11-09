use std::collections::HashSet;
use std::env;
use std::ffi::OsStr;
use std::path::Path;
use std::process::Command;

fn main() {
    let rustc = env::var_os("RUSTC").expect("RUSTC");
    let rustc_output = Command::new(rustc)
        .arg("--version")
        .output()
        .expect("rustc --version");
    if !rustc_output.status.success() {
        panic!("rustc --version: exit status {}", rustc_output.status);
    }
    let rustc_version = &rustc_output.stdout[..];
    let supports_whole_archive =
        !rustc_version.starts_with(b"rustc ") || rustc_version >= &b"rustc 1.61.0"[..];

    println!("cargo:rerun-if-env-changed=DDNET_TEST_LIBRARIES");
    println!("cargo:rerun-if-env-changed=DDNET_TEST_NO_LINK");
    println!("cargo:rerun-if-env-changed=RA_RUSTC_WRAPPER");
    if env::var_os("DDNET_TEST_NO_LINK").is_some() || env::var_os("RA_RUSTC_WRAPPER").is_some() {
        return;
    }
    if env::var_os("CARGO_FEATURE_LINK_TEST_LIBRARIES").is_some() {
        let libraries = env::var("DDNET_TEST_LIBRARIES")
            .expect("environment variable DDNET_TEST_LIBRARIES required but not found");
        let mut seen_library_dirs = HashSet::new();
        for library in libraries.split(';') {
            let library = Path::new(library);
            let extension = library.extension().and_then(OsStr::to_str);
            let kind = match extension {
                Some("framework") => "framework=",
                Some("so") => "dylib=",
                Some("a") => {
                    if supports_whole_archive {
                        "static:-whole-archive="
                    } else {
                        ""
                    }
                }
                _ => "",
            };
            let dir_kind = match extension {
                Some("framework") => "framework=",
                _ => "",
            };
            if let Some(parent) = library.parent() {
                if parent != Path::new("") {
                    let parent = parent.to_str().expect("should have errored earlier");
                    if !seen_library_dirs.contains(&(dir_kind, parent)) {
                        println!("cargo:rustc-link-search={}{}", dir_kind, parent);
                        seen_library_dirs.insert((dir_kind, parent));
                    }
                }
            }
            let mut name = library
                .file_stem()
                .expect("library name")
                .to_str()
                .expect("should have errored earlier");
            if name.starts_with("lib") {
                name = &name[3..];
            }
            println!("cargo:rustc-link-lib={}{}", kind, name);
        }
    }
}
