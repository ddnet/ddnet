use std::fmt;
use std::marker::PhantomData;

pub struct DebugSlice<'a, T: Clone + 'a, D: fmt::Debug, F: Fn(T) -> D> {
    slice: &'a [T],
    f: F,
    phantom: PhantomData<D>,
}

impl<'a, T: Clone + 'a, D: fmt::Debug, F: Fn(T) -> D> DebugSlice<'a, T, D, F> {
    pub fn new(slice: &'a [T], f: F) -> DebugSlice<'a, T, D, F> {
        DebugSlice {
            slice: slice,
            f: f,
            phantom: PhantomData,
        }
    }
}

impl<'a, T: Clone + 'a, D: fmt::Debug, F: Fn(T) -> D> fmt::Debug for DebugSlice<'a, T, D, F> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_list()
            .entries(self.slice.iter().cloned().map(&self.f))
            .finish()
    }
}
