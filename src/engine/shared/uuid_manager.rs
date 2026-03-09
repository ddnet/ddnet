use std::fmt;
use std::str::FromStr;
use uuid::Uuid;

/// A Universally Unique Identifier (UUID).
///
/// They're used to identify protocol extensions in DDNet.
#[derive(Clone, Copy, Default, PartialEq)]
#[repr(C)]
pub struct CUuid {
    data: [u8; 16],
}

unsafe impl cxx::ExternType for CUuid {
    type Id = cxx::type_id!("CUuid");
    type Kind = cxx::kind::Trivial;
}

/// UUIDs are given IDs starting here.
///
/// See [`CUuidManager::RegisterName`] for details.
pub const OFFSET_UUID: i32 = 1 << 16;

pub use self::ffi::CUuidManager;
pub use self::ffi::CUuidManager_Global;
pub use self::ffi::CUuidManager_New;
pub use self::ffi::CalculateUuid;
pub use self::ffi::RandomUuid;

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("base/rust.h");
        include!("engine/shared/uuid_manager.h");

        type StrRef<'a> = ddnet_base::StrRef<'a>;
        type CUuid = super::CUuid;

        /// Used to manage a mapping between integer IDs and UUIDs for protocol
        /// extensions.
        ///
        /// IDs can change when protocol extensions are added or removed, but
        /// the UUIDs stay constant. This class can be used to translate one
        /// into the other.
        type CUuidManager;

        /// Generate a random UUIDv4.
        ///
        /// ```no_run
        /// # // TODO: this test fails on windows-latest-mingw with a segmentation fault
        /// # extern crate ddnet_test;
        /// # use ddnet_engine_shared::CUuid;
        /// # use ddnet_engine_shared::RandomUuid;
        /// let uuid: CUuid = RandomUuid();
        ///
        /// // Something like 48a9c093-94a5-41d8-a7e1-e42f6bff7bbb.
        /// //                             ^^
        /// assert_eq!(&uuid.to_string()[13..15], "-4")
        /// ```
        pub fn RandomUuid() -> CUuid;

        /// Generate a UUIDv3 using the Teeworlds namespace UUID.
        ///
        /// The Teeworlds namespace UUID is e05ddaaa-c4e6-4cfb-b642-5d48e80c0029.
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::s;
        /// # use ddnet_engine_shared::CUuid;
        /// # use ddnet_engine_shared::CalculateUuid;
        /// let uuid: CUuid = CalculateUuid(s!("character@netobj.ddnet.tw"));
        /// assert_eq!(uuid.to_string(), "76ce455b-f9eb-3a48-add7-e04b941d045c")
        /// ```
        pub fn CalculateUuid(name: StrRef<'_>) -> CUuid;

        /// Register a UUID by name with the specified ID.
        ///
        /// The IDs must be registered in order starting from [`OFFSET_UUID`].
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::s;
        /// # use ddnet_engine_shared::CUuid;
        /// # use ddnet_engine_shared::CUuidManager_New;
        /// # use ddnet_engine_shared::CalculateUuid;
        /// # use ddnet_engine_shared::OFFSET_UUID;
        /// const EXAMPLE: i32 = OFFSET_UUID;
        /// let example_uuid: CUuid = CalculateUuid(s!("example@ddnet.org"));
        ///
        /// let mut uuid_manager = CUuidManager_New();
        /// uuid_manager.pin_mut().RegisterName(EXAMPLE, s!("example@ddnet.org"));
        /// assert_eq!(uuid_manager.GetUuid(EXAMPLE), example_uuid);
        /// assert_eq!(uuid_manager.GetName(EXAMPLE), "example@ddnet.org");
        /// assert_eq!(uuid_manager.LookupUuid(example_uuid), EXAMPLE);
        /// ```
        pub fn RegisterName(self: Pin<&mut CUuidManager>, id: i32, name: StrRef<'_>);

        /// Get a UUID from an ID.
        ///
        /// See [`CUuidManager::LookupUuid`] or [`CUuidManager::RegisterName`]
        /// for an example.
        pub fn GetUuid(self: &CUuidManager, id: i32) -> CUuid;

        /// Get a name from an ID.
        ///
        /// See [`CUuidManager::LookupUuid`] or [`CUuidManager::RegisterName`]
        /// for an example.
        pub fn GetName(self: &CUuidManager, id: i32) -> StrRef<'_>;

        /// Check if a UUID is registered, return its ID if it is.
        ///
        /// Otherwise, return -1.
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::s;
        /// # use ddnet_engine_shared::CUuid;
        /// # use ddnet_engine_shared::CUuidManager_Global;
        /// let uuid_manager = CUuidManager_Global();
        /// let uuid: CUuid = "76ce455b-f9eb-3a48-add7-e04b941d045c".parse().unwrap();
        /// let uuid_id = uuid_manager.LookupUuid(uuid);
        /// assert_ne!(uuid_id, -1);
        /// assert_eq!(uuid_manager.GetUuid(uuid_id), uuid);
        /// assert_eq!(uuid_manager.GetName(uuid_id).to_str(), "character@netobj.ddnet.tw");
        ///
        /// let nil: CUuid = "00000000-0000-0000-0000-000000000000".parse().unwrap();
        /// assert_eq!(uuid_manager.LookupUuid(nil), -1);
        /// ```
        pub fn LookupUuid(self: &CUuidManager, uuid: CUuid) -> i32;

        /// Gets the number of UUIDs registered to this UUID manager.
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::s;
        /// # use ddnet_engine_shared::CUuidManager_Global;
        /// let uuid_manager = CUuidManager_Global();
        /// assert!(uuid_manager.NumUuids() > 1);
        /// ```
        pub fn NumUuids(self: &CUuidManager) -> i32;

        /// Dumps a list of all UUIDs known to this manager to the log.
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::s;
        /// # use ddnet_engine_shared::CUuidManager_Global;
        /// let uuid_manager = CUuidManager_Global();
        /// uuid_manager.DebugDump();
        /// ```
        pub fn DebugDump(self: &CUuidManager);

        /// Get the global UUID manager with all extension UUIDs known the program.
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::s;
        /// # use ddnet_engine_shared::CUuidManager_New;
        /// # use ddnet_engine_shared::OFFSET_UUID;
        /// let uuid_manager = CUuidManager_New();
        /// assert_eq!(uuid_manager.NumUuids(), 0);
        /// ```
        // TODO(cxx 1.0.164): #[Self = "CUuidManager"]
        pub fn CUuidManager_New() -> UniquePtr<CUuidManager>;

        /// Get the global UUID manager with all extension UUIDs known the program.
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::s;
        /// # use ddnet_engine_shared::CUuidManager_Global;
        /// let uuid_manager = CUuidManager_Global();
        /// assert!(uuid_manager.NumUuids() > 1);
        /// ```
        // TODO(cxx 1.0.164): #[Self = "CUuidManager"]
        pub fn CUuidManager_Global() -> &'static CUuidManager;
    }
}

impl From<CUuid> for Uuid {
    fn from(uuid: CUuid) -> Uuid {
        Uuid::from_bytes(uuid.data)
    }
}

impl From<Uuid> for CUuid {
    fn from(uuid: Uuid) -> CUuid {
        CUuid {
            data: *uuid.as_bytes(),
        }
    }
}

impl FromStr for CUuid {
    type Err = uuid::Error;
    fn from_str(s: &str) -> Result<CUuid, uuid::Error> {
        Uuid::from_str(s).map(CUuid::from)
    }
}

impl fmt::Debug for CUuid {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(self, f)
    }
}

impl fmt::Display for CUuid {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let uuid: Uuid = (*self).into();
        uuid.fmt(f)
    }
}
