pub struct MapIterator<T, D, I: Iterator> {
    data: D,
    iterator: I,
    // `map` is already an function of an iterator, so we can't use `map` as a
    // name here.
    map_fn: fn(I::Item, &mut D) -> T,
}

impl<T, D, I: Iterator> MapIterator<T, D, I> {
    pub fn new(data: D, iterator: I, map_fn: fn(I::Item, &mut D) -> T) -> MapIterator<T, D, I> {
        MapIterator {
            data: data,
            iterator: iterator,
            map_fn: map_fn,
        }
    }
}

impl<T, D, I: Iterator> Iterator for MapIterator<T, D, I> {
    type Item = T;
    fn next(&mut self) -> Option<T> {
        self.iterator
            .next()
            .map(|x| (self.map_fn)(x, &mut self.data))
    }
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iterator.size_hint()
    }
}
