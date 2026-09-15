use crate::secure_random;
use crate::Error;
use foreign_types_shared::ForeignType as _;
use foreign_types_shared::ForeignTypeRef as _;
use std::fmt;
use std::iter;
use std::ptr;
use std::str::FromStr;

#[derive(Clone, Copy, Eq, PartialEq)]
pub struct Identity([u8; 32]);

impl fmt::Display for Identity {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(self, f)
    }
}

impl fmt::Debug for Identity {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for b in self.0 {
            write!(f, "{:02x}", b)?;
        }
        Ok(())
    }
}

impl FromStr for Identity {
    type Err = Error;
    fn from_str(v: &str) -> Result<Identity, Error> {
        Ok(Identity::from_bytes(hex_to_32_bytes(v)?))
    }
}

pub struct PrivateIdentity {
    lib: boring::pkey::PKey<boring::pkey::Private>,
}

impl FromStr for PrivateIdentity {
    type Err = Error;
    fn from_str(v: &str) -> Result<PrivateIdentity, Error> {
        Ok(PrivateIdentity::from_bytes(hex_to_32_bytes(v)?))
    }
}

impl Identity {
    pub fn from_bytes(bytes: [u8; 32]) -> Identity {
        Identity(bytes)
    }
    pub fn try_from_lib<T: boring::pkey::HasPublic>(
        key: &boring::pkey::PKeyRef<T>,
    ) -> Option<Identity> {
        if key.id() != boring::pkey::Id::ED25519 {
            return None;
        }
        unsafe {
            let mut buf = [0; 32];
            let mut len = buf.len();
            // TODO: expose this from the `boring` crate
            if boring_sys::EVP_PKEY_get_raw_public_key(
                key.as_ptr(),
                buf.as_mut_ptr(),
                &mut len,
            ) != 1
            {
                return None;
            }
            if len != buf.len() {
                return None;
            }
            Some(Identity::from_bytes(buf))
        }
    }
    pub fn to_lib(&self) -> boring::pkey::PKey<boring::pkey::Public> {
        unsafe {
            // TODO: expose this from the `boring` crate
            let result = boring_sys::EVP_PKEY_new_raw_public_key(
                boring_sys::EVP_PKEY_ED25519,
                ptr::null_mut(),
                self.0.as_ptr(),
                self.0.len(),
            );
            assert!(!result.is_null());
            boring::pkey::PKey::from_ptr(result)
        }
    }
    pub fn as_bytes(&self) -> &[u8; 32] {
        &self.0
    }
}

fn hex_to_32_bytes(v: &str) -> Result<[u8; 32], Error> {
    let len = v.chars().count();
    if len != 64 {
        bail!("invalid length {}, must be 64", len);
    }
    let mut result = [0; 32];
    // I just want to get string slices with two characters each. :(
    // Sorry for this monstrosity.
    let starts = v
        .char_indices()
        .map(|(i, _)| i)
        .chain(iter::once(v.len()))
        .step_by(2);
    let ends = {
        let mut e = starts.clone();
        e.next();
        e
    };
    for (i, (s, e)) in starts.zip(ends).enumerate() {
        result[i] = u8::from_str_radix(&v[s..e], 16).map_err(|_| {
            Error::from_string(format!(
                "non-hex character {:?} at index {}",
                &v[s..e],
                s
            ))
        })?;
    }
    Ok(result)
}

fn random_bignum(bits: i32) -> boring::bn::BigNum {
    let mut result = boring::bn::BigNum::new().unwrap();
    result
        .rand(bits, boring::bn::MsbOption::MAYBE_ZERO, false)
        .unwrap();
    result
}

impl PrivateIdentity {
    pub fn random() -> PrivateIdentity {
        PrivateIdentity::from_bytes(secure_random())
    }
    pub fn public(&self) -> Identity {
        Identity::try_from_lib(&self.lib).unwrap()
    }
    pub fn from_bytes(bytes: [u8; 32]) -> PrivateIdentity {
        PrivateIdentity {
            lib: unsafe {
                // TODO: expose this from the `boring` crate
                let result = boring_sys::EVP_PKEY_new_raw_private_key(
                    boring_sys::EVP_PKEY_ED25519,
                    ptr::null_mut(),
                    bytes.as_ptr(),
                    bytes.len(),
                );
                assert!(!result.is_null());
                boring::pkey::PKey::from_ptr(result)
            },
        }
    }
    pub fn as_lib(&self) -> &boring::pkey::PKeyRef<boring::pkey::Private> {
        &self.lib
    }
    pub fn generate_certificate(&self) -> boring::x509::X509 {
        let name = {
            let mut builder = boring::x509::X509Name::builder().unwrap();
            builder
                .append_entry_by_nid(
                    boring::nid::Nid::ORGANIZATIONNAME,
                    "ddnet16-autogen",
                )
                .unwrap();
            builder
                .append_entry_by_nid(
                    boring::nid::Nid::COMMONNAME,
                    &format!("{}", self.public()),
                )
                .unwrap();
            builder
                .append_entry_by_nid(boring::nid::Nid::COMMONNAME, "a")
                .unwrap();
            builder.build()
        };
        let default_md =
            unsafe { boring::hash::MessageDigest::from_ptr(ptr::null()) };

        let mut builder = boring::x509::X509::builder().unwrap();
        // TODO: what do we want to strip?
        builder.set_version(2).unwrap(); // 2 means version 3
        builder
            .set_serial_number(&random_bignum(160).to_asn1_integer().unwrap())
            .unwrap();
        builder.set_issuer_name(&name).unwrap();
        builder
            .set_not_before(&boring::asn1::Asn1Time::days_from_now(0).unwrap())
            .unwrap();
        builder
            .set_not_after(&boring::asn1::Asn1Time::days_from_now(7).unwrap())
            .unwrap();
        builder.set_subject_name(&name).unwrap();
        builder.set_pubkey(&self.lib).unwrap();
        // TODO: find a way to set AKID, would need the first parameter of
        // x509_v3_context to be `Some(&builder)`, see apps/req.c:838 of
        // openssl (31157bc0b46e04227b8468d3e6915e4d0332777c).
        //builder.append_extension(boring::x509::extension::SubjectKeyIdentifier::new().build(&builder.x509v3_context(None, None)).unwrap()).unwrap();
        //builder.append_extension(boring::x509::extension::AuthorityKeyIdentifier::new().build(&builder.x509v3_context(None, None)).unwrap()).unwrap();
        builder
            .append_extension(
                boring::x509::extension::BasicConstraints::new()
                    .critical()
                    .ca()
                    .build()
                    .unwrap(),
            )
            .unwrap();
        builder.sign(&self.lib, default_md).unwrap();
        builder.build()
    }
}

#[cfg(test)]
mod test {
    use super::PrivateIdentity;

    #[test]
    fn generate_certificate() {
        let private_identity: PrivateIdentity =
            "89b84bbc4b430a74642a8d6ee9086048318b20090e5a5d0c807aba4ce2c0d22f"
                .parse()
                .unwrap();
        let cert = private_identity.generate_certificate();
        cert.to_pem().unwrap();
    }
}
