use crate::secure_hash;
use crate::secure_random;
use arrayvec::ArrayVec;
use std::io::Write as _;
use std::mem;
use std::net::SocketAddr;

// TODO: include in the token whether it's the old or the new token
pub struct Challenger {
    seed: [u8; 16],
    prev_seed: [u8; 16],
}

impl Challenger {
    pub const TOKEN_LEN: usize = 4;

    pub fn new() -> Challenger {
        Challenger {
            seed: secure_random(),
            prev_seed: secure_random(),
        }
    }
    pub fn reseed(&mut self) {
        self.prev_seed = mem::replace(&mut self.seed, secure_random());
    }
    fn token_from_secret(
        secret: &[u8; 16],
        addr: &SocketAddr,
    ) -> [u8; Challenger::TOKEN_LEN] {
        let mut buf: ArrayVec<[u8; 128]> = ArrayVec::new();
        buf.try_extend_from_slice(secret).unwrap();
        write!(buf, "{}", addr).unwrap();

        let mut result = [0; Challenger::TOKEN_LEN];
        result.copy_from_slice(&secure_hash(&buf)[..Challenger::TOKEN_LEN]);
        result
    }
    pub fn compute_token(
        &self,
        addr: &SocketAddr,
    ) -> [u8; Challenger::TOKEN_LEN] {
        Challenger::token_from_secret(&self.seed, addr)
    }
    pub fn verify_token(
        &self,
        addr: &SocketAddr,
        token: [u8; Challenger::TOKEN_LEN],
    ) -> Result<(), ()> {
        if Challenger::token_from_secret(&self.seed, addr) == token
            || Challenger::token_from_secret(&self.prev_seed, addr) == token
        {
            Ok(())
        } else {
            Err(())
        }
    }
}
