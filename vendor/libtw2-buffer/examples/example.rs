extern crate libtw2_buffer;

use libtw2_buffer::ReadBuffer;
use std::env;
use std::fs::File;
use std::io::Write;

fn main() {
    let filename = env::temp_dir().join("rust_buffer_test.txt");
    {
        let mut f = File::create(&filename).unwrap();
        f.write(&[1; 256]).unwrap();
    }
    {
        let mut contents = Vec::with_capacity(1);
        let mut f = File::open(&filename).unwrap();
        loop {
            let len = f.read_buffer(&mut contents).unwrap().len();
            println!("{:3} {:3}/{:3}", len, contents.len(), contents.capacity());
            if len == 0 {
                break;
            }
            if contents.len() == contents.capacity() {
                let len = contents.len();
                contents.reserve(len);
            }
        }
    }
}
