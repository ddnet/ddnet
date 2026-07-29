use std::env;
use std::fs;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    let mut out = PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR"));
    out.push("rustc-version");
    let rustc = env::var_os("RUSTC").expect("RUSTC");
    let rustc_output = Command::new(rustc)
        .arg("--version")
        .output()
        .expect("rustc --version");
    if !rustc_output.status.success() {
        panic!("rustc --version: exit status {}", rustc_output.status);
    }
    let rustc_version = rustc_output
        .stdout
        .strip_suffix(b"\n")
        .expect("rustc version output ends with newline");
    assert!(!rustc_version.contains(&b'\n'));
    fs::write(&out, rustc_version).expect("file write");
}
