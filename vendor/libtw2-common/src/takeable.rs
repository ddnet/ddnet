use std::ops;

pub struct Takeable<T> {
    inner: Option<T>,
}

impl<T: Default> Default for Takeable<T> {
    fn default() -> Takeable<T> {
        Takeable::new(Default::default())
    }
}

impl<T> Takeable<T> {
    pub fn new(value: T) -> Takeable<T> {
        Takeable { inner: Some(value) }
    }
    pub fn empty() -> Takeable<T> {
        Takeable { inner: None }
    }
    pub fn take(&mut self) -> T {
        self.inner
            .take()
            .unwrap_or_else(|| panic!("value taken when absent"))
    }
    pub fn restore(&mut self, value: T) {
        assert!(self.inner.is_none(), "value restored when already present");
        self.inner = Some(value);
    }
}

impl<T> ops::Deref for Takeable<T> {
    type Target = T;
    fn deref(&self) -> &T {
        self.inner
            .as_ref()
            .unwrap_or_else(|| panic!("value borrowed when absent"))
    }
}

impl<T> ops::DerefMut for Takeable<T> {
    fn deref_mut(&mut self) -> &mut T {
        self.inner
            .as_mut()
            .unwrap_or_else(|| panic!("value mutably borrowed when absent"))
    }
}
