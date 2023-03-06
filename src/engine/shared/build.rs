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
    fs::write(&out, rustc_output.stdout).expect("file write");
}
