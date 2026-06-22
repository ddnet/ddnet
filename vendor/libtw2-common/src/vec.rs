use crate::relative_size_of_mult;
use crate::slice;
use std::mem;

pub unsafe fn transmute<T, U>(vec: Vec<T>) -> Vec<U> {
    slice::transmute::<T, U>(&vec); // Error checking done there.

    let ptr = vec.as_ptr();
    let len = vec.len();
    let cap = vec.capacity();
    mem::forget(vec);
    let new_cap = cap * mem::size_of::<T>() / mem::size_of::<U>();

    Vec::from_raw_parts(ptr as *mut U, relative_size_of_mult::<T, U>(len), new_cap)
}
