use std::cmp;
use std::ffi::CStr;
use std::fmt;
use std::marker::PhantomData;
use std::ops;
use std::os::raw::c_char;
use std::ptr;
use std::str;

/// User pointer, as used in callbacks. Corresponds to the C++ type `void *`.
///
/// Callbacks in C are usually represented by a function pointer and some
/// "userdata" pointer that is also passed to the function pointer. This allows
/// to hand data to the callback. This type represents such a userdata pointer.
///
/// It is `unsafe` to convert the `UserPtr` back to its original pointer using
/// [`UserPtr::cast`] because its lifetime and type information was lost.
///
/// When dealing with Rust code exclusively, closures are preferred.
///
/// # Examples
///
/// ```
/// use ddnet_base::UserPtr;
///
/// struct CallbackData {
///     favorite_color: &'static str,
/// }
///
/// let data = CallbackData {
///     favorite_color: "green",
/// };
///
/// callback(UserPtr::from(&data));
///
/// fn callback(pointer: UserPtr) {
///     let data: &CallbackData = unsafe { pointer.cast() };
///     println!("favorite color: {}", data.favorite_color);
/// }
/// ```
#[repr(transparent)]
#[derive(Debug, Eq, Ord, PartialEq, PartialOrd)]
pub struct UserPtr(*mut ());

unsafe impl cxx::ExternType for UserPtr {
    type Id = cxx::type_id!("UserPtr");
    type Kind = cxx::kind::Trivial;
}

impl UserPtr {
    /// Create a null `UserPtr`.
    ///
    /// # Examples
    ///
    /// ```
    /// use ddnet_base::UserPtr;
    ///
    /// // Can't do anything useful with this.
    /// let _user = UserPtr::null();
    /// ```
    pub fn null() -> UserPtr {
        UserPtr(ptr::null_mut())
    }

    /// Cast `UserPtr` back to a reference to its real type.
    ///
    /// # Safety
    ///
    /// The caller is responsible for checking type and lifetime correctness.
    /// Also, they must make sure that there are only immutable references or at
    /// most one mutable reference live at the same time.
    ///
    /// # Examples
    ///
    /// ```
    /// use ddnet_base::UserPtr;
    ///
    /// let the_answer = 42;
    /// let user = UserPtr::from(&the_answer);
    ///
    /// assert_eq!(unsafe { *user.cast::<i32>() }, 42);
    /// ```
    pub unsafe fn cast<T>(&self) -> &T {
        &*(self.0 as *const _)
    }

    /// Cast `UserPtr` back to a mutable reference to its real type.
    ///
    /// See [`UserPtr`] documentation for details and an example.
    ///
    /// # Safety
    ///
    /// The caller is responsible for checking type and lifetime correctness.
    /// Also, they must make sure that there are only immutable references or at
    /// most one mutable reference live at the same time.
    ///
    /// # Examples
    ///
    /// ```
    /// use ddnet_base::UserPtr;
    ///
    /// let mut seen_it = false;
    /// let mut user = UserPtr::from(&mut seen_it);
    ///
    /// unsafe {
    ///     *user.cast_mut() = true;
    /// }
    ///
    /// assert_eq!(seen_it, true);
    /// ```
    pub unsafe fn cast_mut<T>(&mut self) -> &mut T {
        &mut *(self.0 as *mut _)
    }
}

impl<'a, T> From<&'a T> for UserPtr {
    fn from(t: &'a T) -> UserPtr {
        UserPtr(t as *const _ as *mut _)
    }
}

impl<'a, T> From<&'a mut T> for UserPtr {
    fn from(t: &'a mut T) -> UserPtr {
        UserPtr(t as *mut _ as *mut _)
    }
}

/// C-style string pointer to UTF-8 data. Corresponds to the C++ type `const
/// char *`.
///
/// The lifetime is the lifetime of the underlying string.
///
/// This is a separate type from [`std::ffi::CStr`] because that type is not
/// FFI-safe and does not guarantee UTF-8.
///
/// In Rust code, [`String`] is preferred. For constructing C strings,
/// [`std::ffi::CString`] or this crate's [`s!`](`crate::s!`) macro can be used.
///
/// # Examples
///
/// ```
/// # fn some_c_function(_: StrRef<'_>) {}
/// use ddnet_base::StrRef;
/// use ddnet_base::s;
/// use std::ffi::CStr;
/// use std::ffi::CString;
/// use std::process;
///
/// some_c_function(CStr::from_bytes_with_nul(b"Hello!\0").unwrap().into());
///
/// let string = CString::new(format!("Current PID is {}.", process::id())).unwrap();
/// some_c_function(string.as_ref().into());
///
/// fn c_function_wrapper(s: &CStr) {
///     some_c_function(s.into());
/// }
///
/// some_c_function(s!("こんにちはＣ言語"));
/// ```
#[repr(transparent)]
#[derive(Eq)]
pub struct StrRef<'a>(*const c_char, PhantomData<&'a ()>);

unsafe impl cxx::ExternType for StrRef<'_> {
    type Id = cxx::type_id!("StrRef");
    type Kind = cxx::kind::Trivial;
}

impl<'a> StrRef<'a> {
    /// Get the wrapped string reference.
    ///
    /// This does the same as the `Deref` implementation, differing only in the
    /// returned lifetime. `Deref`'s return type is bound by `self`'s lifetime,
    /// this returns the more correct and longer lifetime.
    ///
    /// This is an O(n) operation as it needs to calculate the length of a C
    /// string by finding the first NUL byte.
    ///
    /// # Examples
    ///
    /// ```
    /// use ddnet_base::s;
    ///
    /// let str1: &'static str = s!("static string").to_str();
    /// ```
    ///
    /// ```compile_fail
    /// use ddnet_base::s;
    ///
    /// // Wrong lifetime.
    /// let str2: &'static str = &*s!("another static string");
    /// ```
    ///
    pub fn to_str(&self) -> &'a str {
        unsafe { str::from_utf8_unchecked(CStr::from_ptr(self.0).to_bytes()) }
    }
}

impl<'a> From<&'a CStr> for StrRef<'a> {
    fn from(s: &'a CStr) -> StrRef<'a> {
        let bytes = s.to_bytes_with_nul();
        str::from_utf8(bytes).expect("valid UTF-8");
        StrRef(bytes.as_ptr() as *const _, PhantomData)
    }
}

impl fmt::Debug for StrRef<'_> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.to_str().fmt(f)
    }
}

impl fmt::Display for StrRef<'_> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.to_str().fmt(f)
    }
}

impl<'a> cmp::PartialEq for StrRef<'a> {
    fn eq(&self, other: &StrRef<'a>) -> bool {
        self.to_str().eq(other.to_str())
    }
}

impl<'a> cmp::PartialEq<&'a str> for StrRef<'a> {
    fn eq(&self, other: &&'a str) -> bool {
        self.to_str().eq(*other)
    }
}

impl<'a> cmp::PartialOrd for StrRef<'a> {
    fn partial_cmp(&self, other: &StrRef<'a>) -> Option<cmp::Ordering> {
        Some(self.to_str().cmp(other.to_str()))
    }
}

impl<'a> cmp::Ord for StrRef<'a> {
    fn cmp(&self, other: &StrRef<'a>) -> cmp::Ordering {
        self.to_str().cmp(other.to_str())
    }
}

impl ops::Deref for StrRef<'_> {
    type Target = str;
    fn deref(&self) -> &str {
        self.to_str()
    }
}

/// Construct a [`StrRef`] statically.
///
/// # Examples
///
/// ```
/// use ddnet_base::StrRef;
/// use ddnet_base::s;
///
/// let greeting: StrRef<'static> = s!("Hallöchen, C!");
/// let status: StrRef<'static> = s!(concat!("Current file: ", file!()));
/// ```
#[macro_export]
macro_rules! s {
    ($str:expr) => {
        ::ddnet_base::StrRef::from(
            ::std::ffi::CStr::from_bytes_with_nul(::std::concat!($str, "\0").as_bytes()).unwrap(),
        )
    };
}
