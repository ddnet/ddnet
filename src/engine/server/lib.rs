extern crate ddnet_base;
extern crate ddnet_engine_shared;

mod asdf;

#[cxx::bridge]
mod ffi {
    extern "Rust" {
        fn Foo();
    }
}
fn Foo() {
    println!("foobar");
}
