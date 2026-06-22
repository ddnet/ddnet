buffer
======

`buffer` provides safe, write-only and generics-free byte buffers that can be
used without initializing them first.

Example
-------

```rust
let mut vec = Vec::with_capacity(1024);
if try!(reader.read_buffer(&mut vec)).len() != 0 {
    if vec[0] == 0 {
        // ...
    }
}
```
