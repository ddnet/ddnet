use std::mem;
use std::slice;

pub fn relative_size_of_mult<T, U>(mult: usize) -> usize {
    // Panics if mem::size_of::<U>() is 0.
    assert!(mult * mem::size_of::<T>() % mem::size_of::<U>() == 0);
    mult * mem::size_of::<T>() / mem::size_of::<U>()
}

pub fn relative_size_of<T, U>() -> usize {
    relative_size_of_mult::<T, U>(1)
}

pub unsafe fn transmute<T, U>(x: &[T]) -> &[U] {
    assert!(mem::align_of::<T>() % mem::align_of::<U>() == 0);
    slice::from_raw_parts(
        x.as_ptr() as *const U,
        relative_size_of_mult::<T, U>(x.len()),
    )
}

pub unsafe fn transmute_mut<T, U>(x: &mut [T]) -> &mut [U] {
    transmute::<T, U>(x); // For the error checking.
    slice::from_raw_parts_mut(x.as_ptr() as *mut U, relative_size_of_mult::<T, U>(x.len()))
}
