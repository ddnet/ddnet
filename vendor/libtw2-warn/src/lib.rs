//! A simple library for warning callbacks.

#![warn(missing_docs)]

#[macro_use]
extern crate log;

use std::any::Any;
use std::fmt;
use std::marker::PhantomData;

/// Trait for objects that can accept warnings.
pub trait Warn<W> {
    /// This method is the receiver of the warnings.
    fn warn(&mut self, warning: W);
}

impl<W> Warn<W> for Vec<W> {
    fn warn(&mut self, warning: W) {
        self.push(warning);
    }
}

/// Struct that will ignore all the warnings it gets passed.
#[derive(Clone, Copy, Debug, Eq, Ord, Hash, PartialEq, PartialOrd)]
pub struct Ignore;

impl<W> Warn<W> for Ignore {
    fn warn(&mut self, warning: W) {
        let _ = warning;
    }
}

/// Struct that will panic on any warning it encounters.
///
/// This should probably only be used within tests.
#[derive(Clone, Copy, Debug, Eq, Ord, Hash, PartialEq, PartialOrd)]
pub struct Panic;

impl<W: Any + fmt::Debug + Send> Warn<W> for Panic {
    fn warn(&mut self, warning: W) {
        panic!("{:?}", warning);
    }
}

/// Struct that logs each warning it encounters.
///
/// Logging is done via the `log` crate.
#[derive(Clone, Copy, Debug, Eq, Ord, Hash, PartialEq, PartialOrd)]
pub struct Log;

impl<W: fmt::Debug> Warn<W> for Log {
    fn warn(&mut self, warning: W) {
        warn!("{:?}", warning);
    }
}

/// Helper struct for the `rev_map` function.
pub struct RevMap<'a, WF, WT, W: Warn<WT> + 'a, F: FnMut(WF) -> WT> {
    warn: &'a mut W,
    fn_: F,
    phantom: PhantomData<fn(WF) -> WT>,
}

/// Applies a function to all warnings passed to this, before passing it to the
/// underlying warn object.
pub fn rev_map<WF, WT, W, F>(warn: &mut W, fn_: F) -> RevMap<'_, WF, WT, W, F>
where
    W: Warn<WT>,
    F: FnMut(WF) -> WT,
{
    RevMap {
        warn: warn,
        fn_: fn_,
        phantom: PhantomData,
    }
}

impl<'a, WF, WT, W: Warn<WT>, F: FnMut(WF) -> WT> Warn<WF> for RevMap<'a, WF, WT, W, F> {
    fn warn(&mut self, warning: WF) {
        self.warn.warn((self.fn_)(warning));
    }
}

/// Helper struct for the `closure` function.
pub struct Closure<W, F: FnMut(W)> {
    fn_: F,
    phantom: PhantomData<fn(W)>,
}

/// Applies a function to all warnings passed to this object.
pub fn closure<W, F: FnMut(W)>(fn_: &mut F) -> &mut Closure<W, F> {
    unsafe { &mut *(fn_ as *mut _ as *mut _) }
}

impl<W, F: FnMut(W)> Warn<W> for Closure<W, F> {
    fn warn(&mut self, warning: W) {
        (self.fn_)(warning)
    }
}

/// Helper struct for the `wrap` function.
pub struct Wrap<WT, W: Warn<WT>> {
    warn: W,
    phantom: PhantomData<WT>,
}

/// Wraps a `Warn` struct so it can receive more warning types.
pub fn wrap<WT, W: Warn<WT>>(warn: &mut W) -> &mut Wrap<WT, W> {
    unsafe { &mut *(warn as *mut _ as *mut _) }
}

impl<WT, WF: Into<WT>, W: Warn<WT>> Warn<WF> for Wrap<WT, W> {
    fn warn(&mut self, warning: WF) {
        self.warn.warn(warning.into());
    }
}

#[cfg(test)]
mod test {
    use super::Ignore;
    use super::Log;
    use super::Panic;
    use super::Warn;

    const WARNING: &'static str = "unique_string";
    const WARNING2: &'static str = "unique_no2";

    #[test]
    #[should_panic(expected = "unique_string")]
    fn panic() {
        Panic.warn(WARNING);
    }

    #[test]
    #[should_panic(expected = "unique_no2")]
    fn rev_map() {
        super::rev_map(&mut Panic, |_| WARNING2).warn(WARNING);
    }

    #[test]
    #[should_panic(expected = "unique_no2")]
    fn closure_panic() {
        super::closure(&mut |_| panic!("{}", WARNING2)).warn(WARNING);
    }

    #[test]
    fn closure_nopanic() {
        super::closure(&mut |()| panic!("{}", WARNING2));
    }

    #[test]
    fn ignore() {
        Ignore.warn(WARNING);
    }

    #[test]
    fn vec() {
        let mut vec = vec![];
        vec.warn(WARNING);
        assert_eq!(vec, [WARNING]);
    }

    #[test]
    fn log() {
        Log.warn(WARNING);
    }
}
