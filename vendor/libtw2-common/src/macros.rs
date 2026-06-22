#[macro_export]
macro_rules! unwrap_or_return {
    ($e:expr, $r:expr) => {
        match $e {
            Some(e) => e,
            None => return $r,
        }
    };
    ($e:expr) => {
        unwrap_or_return!($e, None)
    };
}

#[macro_export]
macro_rules! unwrap_or {
    ($e:expr, $f:expr) => {
        match $e {
            Some(e) => e,
            None => $f,
        }
    };
}

#[macro_export]
macro_rules! boilerplate_packed {
    ($t:ty, $size:expr, $ts:ident) => {
        #[test]
        fn $ts() {
            assert_eq!(::std::mem::size_of::<$t>(), $size);
        }
        impl ::libtw2_common::bytes::ByteArray for $t {
            type ByteArray = [u8; $size];
        }
    };
}

#[macro_export]
macro_rules! boilerplate_packed_internal {
    ($t:ty, $size:expr, $ts:ident) => {
        #[test]
        fn $ts() {
            assert_eq!(::std::mem::size_of::<$t>(), $size);
        }
        impl crate::bytes::ByteArray for $t {
            type ByteArray = [u8; $size];
        }
    };
}
