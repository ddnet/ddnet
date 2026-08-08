use arc_swap::ArcSwap;
use arrayvec::ArrayString;
use arrayvec::ArrayVec;
use clap::value_t_or_exit;
use clap::App;
use clap::Arg;
use headers::HeaderMapExt as _;
use rand::random;
use serde::Deserialize;
use serde::Serialize;
use serde_json as json;
use sha2::Digest;
use sha2::Sha512_256 as SecureHash;
use std::borrow::Cow;
use std::collections::hash_map;
use std::collections::BTreeMap;
use std::collections::HashMap;
use std::ffi::OsStr;
use std::fmt;
use std::io;
use std::io::Write;
use std::mem;
use std::net::IpAddr;
use std::net::Ipv6Addr;
use std::net::SocketAddr;
use std::panic;
use std::panic::UnwindSafe;
use std::path::Path;
use std::process;
use std::str;
use std::sync;
use std::sync::Arc;
use std::sync::Mutex;
use std::time::Duration;
use std::time::Instant;
use std::time::SystemTime;
use tokio::fs;
use tokio::fs::File;
use tokio::io::AsyncReadExt;
use tokio::time;
use warp::Filter;
use warp::http::StatusCode;

#[macro_use]
extern crate log;

use crate::addr::Addr;
use crate::addr::Protocol;
use crate::addr::RegisterAddr;
use crate::config::Config;
use crate::config::ConfigLocation;
use crate::locations::Location;
use crate::locations::Locations;

// Naming convention: Always use the abbreviation `addr` except in user-facing
// (e.g. serialized) identifiers.
mod addr;
mod community_token;
mod config;
mod locations;

const SERVER_TIMEOUT_SECONDS: u64 = 30;

/// Timeout for delivering a challenge over a websocket connection, covering
/// TCP connect, TLS and websocket handshakes and sending the message.
const WEBSOCKET_CHALLENGE_TIMEOUT: Duration = Duration::from_secs(10);

/// Timeout for just the TCP connect of a websocket challenge, much shorter
/// than `WEBSOCKET_CHALLENGE_TIMEOUT`. Registering an address whose port
/// silently drops connection attempts doesn't require running any service, so
/// the connect would otherwise be the cheapest way to occupy a slot of
/// `WEBSOCKET_CHALLENGE_LIMIT` for the full challenge timeout.
const WEBSOCKET_CHALLENGE_CONNECT_TIMEOUT: Duration = Duration::from_secs(2);

/// Cap on concurrent outbound websocket challenge connections. Unlike the UDP
/// challenge, each websocket challenge holds a socket for up to
/// `WEBSOCKET_CHALLENGE_TIMEOUT`, and registers are unauthenticated, so
/// without a cap an attacker could exhaust file descriptors by registering
/// unreachable websocket addresses. Challenges that don't get a slot are
/// dropped; the game server re-registers and is challenged again.
static WEBSOCKET_CHALLENGE_LIMIT: tokio::sync::Semaphore = tokio::sync::Semaphore::const_new(256);

/// Cap on concurrent websocket challenges to a single IP address, on top of
/// the global `WEBSOCKET_CHALLENGE_LIMIT`. Without it, one host registering
/// unreachable websocket addresses could occupy all the global slots and
/// starve the challenges of every other server. A responsive server releases
/// its slot within milliseconds, so this still allows a hoster to run many
/// servers on a single IP address.
const WEBSOCKET_CHALLENGE_LIMIT_PER_IP: u32 = 8;

/// Number of in-flight websocket challenges per IP address, only containing
/// the IP addresses currently being challenged.
fn websocket_challenges_per_ip() -> sync::MutexGuard<'static, HashMap<IpAddr, u32>> {
    static CHALLENGES: sync::OnceLock<Mutex<HashMap<IpAddr, u32>>> = sync::OnceLock::new();
    CHALLENGES
        .get_or_init(Default::default)
        .lock()
        .unwrap_or_else(|poison| poison.into_inner())
}

/// Key for `WEBSOCKET_CHALLENGE_LIMIT_PER_IP`. Everyone with an IPv6 network
/// routed to them can register from a practically unlimited number of
/// addresses, so IPv6 addresses are counted per /64 network.
fn websocket_challenge_limit_key(ip: IpAddr) -> IpAddr {
    match ip {
        IpAddr::V4(_) => ip,
        IpAddr::V6(ip) => {
            let mut octets = ip.octets();
            octets[8..].fill(0);
            IpAddr::V6(Ipv6Addr::from(octets))
        }
    }
}

/// Slot for a single in-flight websocket challenge to an IP address, released
/// again on drop.
struct WebsocketChallengeIpPermit(IpAddr);

impl WebsocketChallengeIpPermit {
    fn try_acquire(ip: IpAddr) -> Option<WebsocketChallengeIpPermit> {
        let ip = websocket_challenge_limit_key(ip);
        let mut challenges = websocket_challenges_per_ip();
        let num_challenges = challenges.entry(ip).or_insert(0);
        if *num_challenges >= WEBSOCKET_CHALLENGE_LIMIT_PER_IP {
            return None;
        }
        *num_challenges += 1;
        Some(WebsocketChallengeIpPermit(ip))
    }
}

impl Drop for WebsocketChallengeIpPermit {
    fn drop(&mut self) {
        let mut challenges = websocket_challenges_per_ip();
        if let hash_map::Entry::Occupied(mut entry) = challenges.entry(self.0) {
            *entry.get_mut() -= 1;
            if *entry.get() == 0 {
                entry.remove();
            }
        }
    }
}

type ShortString = ArrayString<[u8; 64]>;

#[derive(Debug, Deserialize)]
struct Register {
    address: RegisterAddr,
    secret: ShortString,
    connless_request_token: Option<ShortString>,
    challenge_secret: ShortString,
    challenge_token: Option<ShortString>,
    community_token: Option<ShortString>,
    info_serial: i64,
    info: Option<json::Value>,
}

#[derive(Debug, Deserialize)]
struct Delete {
    address: RegisterAddr,
    secret: ShortString,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "snake_case", tag = "status")]
enum RegisterResponse {
    Success,
    NeedChallenge,
    NeedInfo,
    Error(RegisterError),
}

#[derive(Debug, Serialize)]
struct RegisterError {
    #[serde(skip)]
    status: StatusCode,
    message: Cow<'static, str>,
}

impl RegisterError {
    fn new(s: String) -> RegisterError {
        RegisterError {
            status: StatusCode::BAD_REQUEST,
            message: Cow::Owned(s),
        }
    }
    fn banned(reason: Option<&str>) -> RegisterError {
        RegisterError {
            status: StatusCode::FORBIDDEN,
            message: if let Some(reason) = reason {
                Cow::Owned(format!("banned: {reason}"))
            } else {
                Cow::Borrowed("banned")
            },
        }
    }
    fn websocket_protocols_not_accepted() -> RegisterError {
        RegisterError {
            // Distinct status code so that game servers can tell this
            // permanent rejection apart from errors that are worth retrying,
            // and stop registering their websocket addresses.
            status: StatusCode::NOT_IMPLEMENTED,
            message: Cow::Borrowed("websocket protocols are not accepted by this master server"),
        }
    }
    fn unsupported_media_type() -> RegisterError {
        RegisterError {
            status: StatusCode::UNSUPPORTED_MEDIA_TYPE,
            message: Cow::Borrowed("The request's Content-Type is not supported"),
        }
    }
    fn status(&self) -> StatusCode {
        self.status
    }
}

impl From<&'static str> for RegisterError {
    fn from(s: &'static str) -> RegisterError {
        RegisterError {
            status: StatusCode::BAD_REQUEST,
            message: Cow::Borrowed(s),
        }
    }
}

/// Time in milliseconds since the epoch of the timekeeper.
#[derive(Clone, Copy, Debug, Deserialize, Eq, Ord, PartialEq, PartialOrd, Serialize)]
struct Timestamp(i64);

impl Timestamp {
    fn minus_seconds(self, seconds: u64) -> Timestamp {
        Timestamp(self.0 - seconds.checked_mul(1_000).unwrap() as i64)
    }
    fn difference_added(self, other: Timestamp, base: Timestamp) -> Timestamp {
        Timestamp(self.0 + (other.0 - base.0))
    }
}

#[derive(Clone, Copy)]
struct Timekeeper {
    instant: Instant,
    system: SystemTime,
}

impl Timekeeper {
    fn new() -> Timekeeper {
        Timekeeper {
            instant: Instant::now(),
            system: SystemTime::now(),
        }
    }
    fn now(&self) -> Timestamp {
        Timestamp(self.instant.elapsed().as_millis() as i64)
    }
    fn from_system_time(&self, system: SystemTime) -> Timestamp {
        let difference = if let Ok(d) = system.duration_since(self.system) {
            d.as_millis() as i64
        } else {
            -(self.system.duration_since(system).unwrap().as_millis() as i64)
        };
        Timestamp(difference)
    }
}

#[derive(Debug, Serialize)]
struct SerializedServers<'a> {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub communities: Option<&'a json::value::RawValue>,
    pub servers: Vec<SerializedServer<'a>>,
}

impl<'a> SerializedServers<'a> {
    fn new(communities: Option<&'a json::value::RawValue>) -> SerializedServers<'a> {
        SerializedServers {
            communities,
            servers: Vec::new(),
        }
    }
}

#[derive(Debug, Serialize)]
struct SerializedServer<'a> {
    pub addresses: &'a [Addr],
    #[serde(skip_serializing_if = "Option::is_none")]
    pub community: Option<ShortString>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub location: Option<Location>,
    pub info: &'a json::value::RawValue,
}

impl<'a> SerializedServer<'a> {
    fn new(server: &'a Server, location: Option<Location>) -> SerializedServer<'a> {
        SerializedServer {
            addresses: &server.addresses,
            community: server.community,
            location,
            info: &server.info,
        }
    }
}

#[derive(Deserialize, Serialize)]
struct DumpServer<'a> {
    pub info_serial: i64,
    pub info: Cow<'a, json::value::RawValue>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub community: Option<ShortString>,
}

impl<'a> From<&'a Server> for DumpServer<'a> {
    fn from(server: &'a Server) -> DumpServer<'a> {
        DumpServer {
            info_serial: server.info_serial,
            info: Cow::Borrowed(&server.info),
            community: server.community,
        }
    }
}

/// Deserializes the address map of a `Dump`, skipping addresses that cannot
/// be parsed. This keeps dumps written by newer mastersrv versions with
/// additional protocols readable, instead of failing on the first unknown
/// protocol.
fn deserialize_addresses_lossy<'de, D>(
    deserializer: D,
) -> Result<BTreeMap<Addr, AddrInfo>, D::Error>
where
    D: serde::Deserializer<'de>,
{
    struct LossyVisitor;

    impl<'de> serde::de::Visitor<'de> for LossyVisitor {
        type Value = BTreeMap<Addr, AddrInfo>;

        fn expecting(&self, f: &mut fmt::Formatter) -> fmt::Result {
            f.write_str("a map from addresses to address infos")
        }
        fn visit_map<A>(self, mut map: A) -> Result<Self::Value, A::Error>
        where
            A: serde::de::MapAccess<'de>,
        {
            let mut result = BTreeMap::new();
            while let Some(key) = map.next_key::<Cow<'de, str>>()? {
                match key.parse::<Addr>() {
                    Ok(addr) => {
                        result.insert(addr, map.next_value()?);
                    }
                    Err(_) => {
                        // Addresses of unknown protocols are expected when a
                        // newer mastersrv version wrote the dump; anything
                        // else indicates a corrupt dump.
                        let known_protocol = key
                            .split_once("://")
                            .map_or(false, |(scheme, _)| scheme.parse::<Protocol>().is_ok());
                        if known_protocol {
                            warn!("skipping unparsable address {:?} in dump", key);
                        } else {
                            debug!("skipping unknown address {:?} in dump", key);
                        }
                        let _: serde::de::IgnoredAny = map.next_value()?;
                    }
                }
            }
            Ok(result)
        }
    }

    deserializer.deserialize_map(LossyVisitor)
}

#[derive(Deserialize, Serialize)]
struct Dump<'a> {
    pub now: Timestamp,
    // Use `BTreeMap`s so the serialization is stable.
    #[serde(deserialize_with = "deserialize_addresses_lossy")]
    pub addresses: BTreeMap<Addr, AddrInfo>,
    pub servers: BTreeMap<ShortString, DumpServer<'a>>,
}

impl<'a> Dump<'a> {
    fn new(now: Timestamp, servers: &'a Servers) -> Dump<'a> {
        Dump {
            now,
            addresses: servers
                .addresses
                .iter()
                .map(|(&addr, a_info)| (addr, a_info.clone()))
                .collect(),
            servers: servers
                .servers
                .iter()
                .map(|(&secret, server)| (secret, DumpServer::from(server)))
                .collect(),
        }
    }
    fn fixup_timestamps(&mut self, new_now: Timestamp) {
        let self_now = self.now;
        let translate_timestamp = |ts| new_now.difference_added(ts, self_now);
        self.now = translate_timestamp(self.now);
        for a_info in self.addresses.values_mut() {
            a_info.ping_time = translate_timestamp(a_info.ping_time);
        }
    }
}

#[derive(Clone, Copy, Deserialize, Eq, Ord, PartialEq, PartialOrd, Serialize)]
#[serde(rename_all = "snake_case")]
enum EntryKind {
    Backcompat,
    Mastersrv,
}

#[derive(Clone, Deserialize, Serialize)]
struct AddrInfo {
    kind: EntryKind,
    ping_time: Timestamp,
    #[serde(skip_serializing_if = "Option::is_none")]
    location: Option<Location>,
    secret: ShortString,
}

struct Challenge {
    current: ShortString,
    prev: ShortString,
}

impl Challenge {
    fn is_valid(&self, challenge: &str) -> bool {
        challenge == &self.current || challenge == &self.prev
    }
    fn current(&self) -> &str {
        &self.current
    }
}

struct Challenger {
    seed: [u8; 16],
    prev_seed: [u8; 16],
}

impl Challenger {
    fn new() -> Challenger {
        Challenger {
            seed: random(),
            prev_seed: random(),
        }
    }
    fn reseed(&mut self) {
        self.prev_seed = mem::replace(&mut self.seed, random());
    }
    fn for_addr(&self, addr: &Addr) -> Challenge {
        fn hash(seed: &[u8], addr: &[u8]) -> ShortString {
            let mut hash = SecureHash::new();
            hash.update(addr);
            hash.update(seed);
            let mut buf = [0; 64];
            let len =
                base64::encode_config_slice(&hash.finalize()[..16], base64::STANDARD, &mut buf);
            ShortString::from(str::from_utf8(&buf[..len]).unwrap()).unwrap()
        }
        let mut buf: ArrayVec<[u8; 128]> = ArrayVec::new();
        write!(&mut buf, "{}", addr).unwrap();
        Challenge {
            current: hash(&self.seed, &buf),
            prev: hash(&self.prev_seed, &buf),
        }
    }
}

struct Shared<'a> {
    challenger: &'a Mutex<Challenger>,
    config: &'a Config,
    servers: &'a Mutex<Servers>,
    socket: &'a Arc<tokio::net::UdpSocket>,
    timekeeper: Timekeeper,
    accept_websocket_protocols: bool,
}

impl<'a> Shared<'a> {
    fn challenge_for_addr(&self, addr: &Addr) -> Challenge {
        self.challenger
            .lock()
            .unwrap_or_else(|poison| poison.into_inner())
            .for_addr(addr)
    }
    fn lock_servers(&'a self) -> sync::MutexGuard<'a, Servers> {
        self.servers
            .lock()
            .unwrap_or_else(|poison| poison.into_inner())
    }
}

/// Maintains a mapping from server addresses to server info.
///
/// Also maintains a mapping from addresses to corresponding server addresses.
#[derive(Clone)]
struct Servers {
    pub addresses: HashMap<Addr, AddrInfo>,
    pub servers: HashMap<ShortString, Server>,
}

enum AddResult {
    Added,
    Refreshed,
    NeedInfo,
    Obsolete,
}

enum RemoveResult {
    Removed,
    NotFound,
}

#[derive(Debug)]
struct FromDumpError;

impl Servers {
    fn new() -> Servers {
        Servers {
            addresses: HashMap::new(),
            servers: HashMap::new(),
        }
    }
    fn add(
        &mut self,
        addr: Addr,
        a_info: AddrInfo,
        info_serial: i64,
        info: Option<Cow<'_, json::value::RawValue>>,
        community: Option<&ShortString>,
    ) -> AddResult {
        let need_info = self
            .servers
            .get(&a_info.secret)
            .map(|entry| info_serial > entry.info_serial)
            .unwrap_or(true);
        if need_info && info.is_none() {
            return AddResult::NeedInfo;
        }
        let insert_addr;
        let secret = a_info.secret;
        match self.addresses.entry(addr) {
            hash_map::Entry::Vacant(v) => {
                v.insert(a_info);
                insert_addr = true;
            }
            hash_map::Entry::Occupied(mut o) => {
                if a_info.kind < o.get().kind {
                    // Don't replace masterserver entries with stuff from backcompat.
                    return AddResult::Obsolete;
                }
                if a_info.ping_time < o.get().ping_time {
                    // Don't replace address info with older one.
                    return AddResult::Obsolete;
                }
                let old = o.insert(a_info);
                insert_addr = old.secret != secret;
                if insert_addr {
                    let server = self.servers.get_mut(&old.secret).unwrap();
                    server.addresses.retain(|&a| a != addr);
                    if server.addresses.is_empty() {
                        assert!(self.servers.remove(&old.secret).is_some());
                    }
                }
            }
        }
        match self.servers.entry(secret) {
            hash_map::Entry::Vacant(v) => {
                assert!(insert_addr);
                v.insert(Server {
                    addresses: vec![addr],
                    info_serial,
                    info: info.unwrap().into_owned(),
                    community: community.cloned(),
                });
            }
            hash_map::Entry::Occupied(mut o) => {
                let server = &mut o.get_mut();
                if insert_addr {
                    server.addresses.push(addr);
                    server.addresses.sort_unstable();
                }
                if info_serial > server.info_serial {
                    server.info_serial = info_serial;
                    server.info = info.unwrap().into_owned();
                    server.community = community.cloned();
                }
            }
        }
        if insert_addr {
            AddResult::Added
        } else {
            AddResult::Refreshed
        }
    }
    fn remove(&mut self, addr: Addr, secret: ShortString) -> RemoveResult {
        match self.addresses.get(&addr) {
            Some(a_info) if secret == a_info.secret => {}
            _ => return RemoveResult::NotFound,
        }
        assert!(self.addresses.remove(&addr).is_some());
        let server = self.servers.get_mut(&secret).unwrap();
        server.addresses.retain(|&a| a != addr);
        if server.addresses.is_empty() {
            assert!(self.servers.remove(&secret).is_some());
        }
        RemoveResult::Removed
    }
    fn prune_before(&mut self, time: Timestamp, log: bool) {
        let mut remove = Vec::new();
        for (&addr, a_info) in &self.addresses {
            if a_info.ping_time < time {
                remove.push(addr);
            }
        }
        for addr in remove {
            if log {
                debug!("removing {} due to timeout", addr);
            }
            let secret = self.addresses.remove(&addr).unwrap().secret;
            let server = self.servers.get_mut(&secret).unwrap();
            server.addresses.retain(|&a| a != addr);
            if server.addresses.is_empty() {
                assert!(self.servers.remove(&secret).is_some());
            }
        }
    }
    fn merge(&mut self, other: &Dump) {
        for (&addr, a_info) in &other.addresses {
            let server = &other.servers[&*a_info.secret];
            self.add(
                addr,
                a_info.clone(),
                server.info_serial,
                Some(Cow::Borrowed(&server.info)),
                server.community.as_ref(),
            );
        }
    }
    fn from_dump(dump: Dump) -> Result<Servers, FromDumpError> {
        let mut result = Servers {
            addresses: dump.addresses.into_iter().collect(),
            servers: dump
                .servers
                .into_iter()
                .map(|(secret, server)| {
                    (
                        secret,
                        Server {
                            addresses: vec![],
                            info_serial: server.info_serial,
                            info: server.info.into_owned(),
                            community: server.community,
                        },
                    )
                })
                .collect(),
        };
        // Fix up addresses in `Server` struct -- they're not serialized into a
        // `Dump`.
        for (&addr, a_info) in &result.addresses {
            result
                .servers
                .get_mut(&a_info.secret)
                .ok_or(FromDumpError)?
                .addresses
                .push(addr);
        }
        // Servers without addresses occur when the lossy address
        // deserialization skipped all their addresses because they use
        // unknown protocols. Drop them instead of rejecting the whole dump.
        let num_servers = result.servers.len();
        result
            .servers
            .retain(|_, server| !server.addresses.is_empty());
        let num_dropped = num_servers - result.servers.len();
        if num_dropped != 0 {
            info!(
                "dropped {} servers without addresses from dump",
                num_dropped
            );
        }
        for server in result.servers.values_mut() {
            server.addresses.sort_unstable();
        }
        Ok(result)
    }
}

#[derive(Clone, Debug)]
struct Server {
    pub addresses: Vec<Addr>,
    pub info_serial: i64,
    pub info: Box<json::value::RawValue>,
    pub community: Option<ShortString>,
}

async fn handle_periodic_reseed(challenger: Arc<Mutex<Challenger>>) {
    loop {
        tokio::time::sleep(Duration::from_secs(3600)).await;
        challenger
            .lock()
            .unwrap_or_else(|poison| poison.into_inner())
            .reseed();
    }
}

async fn read_dump(path: &Path, timekeeper: Timekeeper) -> Result<Dump<'static>, io::Error> {
    let mut buffer = Vec::new();
    let timestamp = timekeeper.from_system_time(fs::metadata(&path).await?.modified().unwrap());
    buffer.clear();
    File::open(&path).await?.read_to_end(&mut buffer).await?;
    let mut dump: Dump = json::from_slice(&buffer)?;
    dump.fixup_timestamps(timestamp);
    Ok(dump)
}

async fn read_dump_dir(path: &Path, timekeeper: Timekeeper) -> Vec<Dump<'static>> {
    let mut dir_entries = fs::read_dir(path).await.unwrap();
    let mut dumps = Vec::new();
    while let Some(entry) = dir_entries.next_entry().await.unwrap() {
        let path = entry.path();
        if path.extension() != Some(OsStr::new("json")) {
            continue;
        }
        let dump = match read_dump(&path, timekeeper).await {
            Ok(dump) => dump,
            Err(e) => {
                error!("couldn't read dump {}: {}", path.display(), e);
                continue;
            }
        };
        dumps.push((path, dump));
    }
    dumps.sort_unstable_by(|(path1, _), (path2, _)| path1.cmp(path2));
    dumps.into_iter().map(|(_, dump)| dump).collect()
}

async fn overwrite_atomically(
    filename: &str,
    temp_filename: &str,
    content: &[u8],
) -> io::Result<()> {
    fs::write(temp_filename, content).await?;
    fs::rename(temp_filename, filename).await?;
    Ok(())
}

fn servers_json(config: &Config, now: Timestamp, mut servers: Servers) -> String {
    let mut serialized = SerializedServers::new(config.communities.as_deref());
    servers.prune_before(now.minus_seconds(SERVER_TIMEOUT_SECONDS), false);
    serialized.servers.extend(servers.servers.values().map(|s| {
        // Get the location of the first registered address. Since
        // the addresses are kept sorted, this is stable.
        let location = s
            .addresses
            .iter()
            .filter_map(|addr| servers.addresses[addr].location)
            .next();
        SerializedServer::new(s, location)
    }));
    serialized.servers.sort_by_key(|s| s.addresses);
    json::to_string(&serialized).unwrap() + "\n"
}

async fn handle_test_servers_json(
    config: Arc<Config>,
    servers: Arc<Mutex<Servers>>,
    dumps_dir: Option<String>,
    timekeeper: Timekeeper,
) -> String {
    let now = timekeeper.now();
    let mut servers = servers
        .lock()
        .unwrap_or_else(|poison| poison.into_inner())
        .clone();
    let other_dumps = match &dumps_dir {
        Some(dir) => read_dump_dir(Path::new(dir), timekeeper).await,
        None => Vec::new(),
    };
    for other_dump in &other_dumps {
        servers.merge(other_dump);
    }
    servers_json(&config, now, servers)
}

async fn handle_periodic_writeout(
    config: Arc<ArcSwap<Config>>,
    servers: Arc<Mutex<Servers>>,
    dumps_dir: Option<String>,
    dump_filename: Option<String>,
    addresses_filename: Option<String>,
    servers_filename: String,
    timekeeper: Timekeeper,
) {
    let dump_filename = dump_filename.map(|f| {
        let tmp = format!("{}.tmp.{}", f, process::id());
        (f, tmp)
    });
    let addresses_filename = addresses_filename.map(|f| {
        let tmp = format!("{}.tmp.{}", f, process::id());
        (f, tmp)
    });
    let servers_filename_temp = &format!("{}.tmp.{}", servers_filename, process::id());

    let start = Instant::now();
    let mut iteration = 0;

    loop {
        let now = timekeeper.now();
        let config = config.load();
        let mut servers = {
            let mut servers = servers.lock().unwrap_or_else(|poison| poison.into_inner());
            servers.prune_before(now.minus_seconds(SERVER_TIMEOUT_SECONDS), true);
            servers.clone()
        };
        if let Some((filename, filename_temp)) = &dump_filename {
            let json = json::to_string(&Dump::new(now, &servers)).unwrap() + "\n";
            overwrite_atomically(filename, filename_temp, json.as_bytes())
                .await
                .unwrap();
        }
        {
            let other_dumps = match &dumps_dir {
                Some(dir) => read_dump_dir(Path::new(dir), timekeeper).await,
                None => Vec::new(),
            };
            if let Some((filename, filename_temp)) = &addresses_filename {
                let mut non_backcompat_addrs: Vec<Addr> = Vec::new();
                non_backcompat_addrs.extend(servers.addresses.keys());
                let oldest = now.minus_seconds(SERVER_TIMEOUT_SECONDS);
                for other_dump in &other_dumps {
                    non_backcompat_addrs.extend(
                        other_dump
                            .addresses
                            .iter()
                            .filter(|(_, a_info)| {
                                a_info.kind != EntryKind::Backcompat && a_info.ping_time >= oldest
                            })
                            .map(|(addr, _)| addr),
                    );
                }
                non_backcompat_addrs.sort_unstable();
                non_backcompat_addrs.dedup();
                let json = json::to_string(&non_backcompat_addrs).unwrap() + "\n";
                overwrite_atomically(filename, filename_temp, json.as_bytes())
                    .await
                    .unwrap();
            }
            for other_dump in &other_dumps {
                servers.merge(other_dump);
            }
            drop(other_dumps);
            let json = servers_json(&config, now, servers);
            overwrite_atomically(&servers_filename, servers_filename_temp, json.as_bytes())
                .await
                .unwrap();
        }
        let elapsed = start.elapsed();
        if elapsed.as_secs() <= iteration {
            let remaining_ns = 1_000_000_000 - elapsed.subsec_nanos();
            time::sleep(Duration::new(0, remaining_ns)).await;
            iteration += 1;
        } else {
            iteration = elapsed.as_secs();
        }
    }
}

async fn handle_config_reread(config_location: ConfigLocation, config: Arc<ArcSwap<Config>>) {
    #[cfg(not(unix))]
    {
        use std::future;

        // Do nothing. Config rereading isn't implemented on non-Unix OSs.
        future::pending().await
    }

    #[cfg(unix)]
    {
        use tokio::signal::unix::signal;
        use tokio::signal::unix::SignalKind;

        let mut sighup = signal(SignalKind::hangup()).unwrap();
        loop {
            sighup.recv().await.unwrap();

            // This is theoretically blocking, but it should™ be fine.
            match config_location.read() {
                Err(e) => {
                    error!("error re-reading config: {}", e);
                    continue;
                }
                Ok(new_config) => {
                    config.store(Arc::new(new_config));
                    info!("successfully reloaded config");
                }
            }
        }
    }
}

/// TLS connector for wss challenges, built once on the first successful root
/// certificate load. Letting tokio-tungstenite build its own connector would
/// reload and reparse the system root certificates for every single
/// connection. Returns `None` when no root certificates could be loaded;
/// that result is not cached, so a transient problem with the certificate
/// store doesn't break wss challenges for the rest of the process lifetime.
fn websocket_tls_connector() -> Option<tokio_tungstenite::Connector> {
    static CONNECTOR: sync::OnceLock<tokio_tungstenite::Connector> = sync::OnceLock::new();
    if let Some(connector) = CONNECTOR.get() {
        return Some(connector.clone());
    }
    let rustls_native_certs::CertificateResult { certs, errors, .. } =
        rustls_native_certs::load_native_certs();
    if !errors.is_empty() {
        warn!("native root CA certificate loading errors: {:?}", errors);
    }
    let mut root_store = rustls::RootCertStore::empty();
    let (added, ignored) = root_store.add_parsable_certificates(certs);
    debug!(
        "added {} native root certificates (ignored {})",
        added, ignored
    );
    if added == 0 {
        warn!("no native root CA certificates loaded, wss challenges will fail");
        return None;
    }
    let connector = tokio_tungstenite::Connector::Rustls(Arc::new(
        rustls::ClientConfig::builder()
            .with_root_certificates(root_store)
            .with_no_client_auth(),
    ));
    Some(CONNECTOR.get_or_init(|| connector).clone())
}

async fn send_challenge_websocket(
    secure: bool,
    target: SocketAddr,
    packet: Vec<u8>,
) -> Result<(), tokio_tungstenite::tungstenite::Error> {
    use futures_util::SinkExt as _;
    use tokio_tungstenite::tungstenite::Error as WsError;

    let url = format!("{}://{}/", if secure { "wss" } else { "ws" }, target);
    // Connect separately from the handshake, so that the connect gets its own,
    // much shorter timeout.
    let stream = time::timeout(
        WEBSOCKET_CHALLENGE_CONNECT_TIMEOUT,
        tokio::net::TcpStream::connect(target),
    )
    .await
    .map_err(|_| WsError::Io(io::Error::new(io::ErrorKind::TimedOut, "connect timed out")))??;
    let connector = if secure {
        websocket_tls_connector().ok_or_else(|| {
            WsError::Io(io::Error::new(
                io::ErrorKind::Other,
                "no TLS root certificates available",
            ))
        })?
    } else {
        tokio_tungstenite::Connector::Plain
    };
    let (mut websocket, _response) =
        tokio_tungstenite::client_async_tls_with_config(&url, stream, None, Some(connector))
            .await?;
    websocket
        .send(tokio_tungstenite::tungstenite::Message::Binary(packet))
        .await?;
    // The challenge token has already been delivered at this point; a failing
    // close handshake (e.g. the game server dropping the TCP connection right
    // after reading the token) doesn't mean the challenge failed.
    if let Err(e) = websocket.close(None).await {
        debug!(
            "websocket close after challenge to {} failed: {}",
            target, e
        );
    }
    Ok(())
}

async fn send_challenge(
    protocol: Protocol,
    connless_request_token_7: Option<[u8; 4]>,
    socket: Arc<tokio::net::UdpSocket>,
    target: SocketAddr,
    challenge_secret: ShortString,
    challenge: ShortString,
) {
    let mut packet = Vec::with_capacity(128);
    if let Some(t) = connless_request_token_7 {
        packet.extend_from_slice(b"\x21");
        packet.extend_from_slice(&t);
        packet.extend_from_slice(b"\xff\xff\xff\xff\xff\xff\xff\xffchal");
    } else {
        packet.extend_from_slice(b"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xffchal");
    }
    packet.extend_from_slice(challenge_secret.as_bytes());
    packet.push(0);
    packet.extend_from_slice(challenge.as_bytes());
    packet.push(0);
    match protocol {
        Protocol::V5 | Protocol::V6 | Protocol::V7 => {
            socket.send_to(&packet, target).await.unwrap();
        }
        Protocol::Ws | Protocol::Wss => {
            // The challenge is delivered over an actual websocket connection.
            // For wss, the TLS certificate is verified against the system
            // roots, so registration also proves that browsers can connect.
            let _ip_permit = match WebsocketChallengeIpPermit::try_acquire(target.ip()) {
                Some(permit) => permit,
                None => {
                    info!(
                        "too many concurrent websocket challenges to the same IP address, dropping challenge to {}",
                        target
                    );
                    return;
                }
            };
            let _permit = match WEBSOCKET_CHALLENGE_LIMIT.try_acquire() {
                Ok(permit) => permit,
                Err(_) => {
                    info!(
                        "too many concurrent websocket challenges, dropping challenge to {}",
                        target
                    );
                    return;
                }
            };
            let secure = protocol == Protocol::Wss;
            match time::timeout(
                WEBSOCKET_CHALLENGE_TIMEOUT,
                send_challenge_websocket(secure, target, packet),
            )
            .await
            {
                Ok(Ok(())) => {}
                Ok(Err(e)) => info!("websocket challenge to {} failed: {}", target, e),
                Err(_) => info!("websocket challenge to {} timed out", target),
            }
        }
    }
}

fn handle_register(
    shared: Shared,
    remote_addr: IpAddr,
    register: Register,
) -> Result<RegisterResponse, RegisterError> {
    match register.address.protocol {
        Protocol::Ws | Protocol::Wss if !shared.accept_websocket_protocols => {
            return Err(RegisterError::websocket_protocols_not_accepted());
        }
        _ => {}
    }

    let connless_request_token_7 = match register.address.protocol {
        Protocol::V5 => None,
        Protocol::V6 => None,
        Protocol::Ws => None,
        Protocol::Wss => None,
        Protocol::V7 => {
            let token_hex = register
                .connless_request_token
                .as_ref()
                .ok_or_else(|| "registering with tw-0.7+udp:// requires header Connless-Token")?;
            let mut token = [0; 4];
            hex::decode_to_slice(token_hex.as_bytes(), &mut token).map_err(|e| {
                RegisterError::new(format!("invalid hex in Connless-Request-Token: {}", e))
            })?;
            Some(token)
        }
    };

    let addr = register.address.with_ip(remote_addr);

    if let Some(reason) = shared.config.is_banned(addr) {
        return Err(RegisterError::banned(reason));
    }

    let is_exempt = shared.config.is_exempt_from_port_forward_check(addr);
    let challenge = shared.challenge_for_addr(&addr);
    let correct_challenge = is_exempt
        || register
            .challenge_token
            .as_ref()
            .map(|ct| challenge.is_valid(ct))
            .unwrap_or(false);
    let should_send_challenge = !is_exempt
        && register
            .challenge_token
            .as_ref()
            .map(|ct| ct != challenge.current())
            .unwrap_or(true);

    let community = register
        .community_token
        .and_then(|t| shared.config.community_for_token(&t).transpose())
        .transpose()
        .map_err(RegisterError::new)?
        .map(|c| ArrayString::from(c))
        .transpose()
        .map_err(|_| "invalid community name length")?;

    let result = if correct_challenge {
        let raw_info = register
            .info
            .map(|i| -> Result<_, RegisterError> {
                let info = i.as_object().ok_or("register info must be an object")?;

                // Normalize the JSON to strip any spaces etc.
                Ok(json::value::to_raw_value(&info).unwrap())
            })
            .transpose()?;

        let add_result = shared.lock_servers().add(
            addr,
            AddrInfo {
                kind: EntryKind::Mastersrv,
                ping_time: shared.timekeeper.now(),
                location: shared.config.locations.lookup(addr.ip),
                secret: register.secret,
            },
            register.info_serial,
            raw_info.map(Cow::Owned),
            community.as_ref(),
        );
        match add_result {
            AddResult::Added => debug!("successfully registered {}", addr),
            AddResult::Refreshed => {}
            AddResult::NeedInfo => {}
            AddResult::Obsolete => {
                warn!(
                    "received obsolete entry {}, shouldn't normally happen",
                    addr
                );
            }
        }
        if let AddResult::NeedInfo = add_result {
            RegisterResponse::NeedInfo
        } else {
            RegisterResponse::Success
        }
    } else {
        RegisterResponse::NeedChallenge
    };

    if should_send_challenge {
        if let RegisterResponse::Success = result {
            trace!("re-sending challenge to {}", addr);
        } else {
            trace!("sending challenge to {}", addr);
        }
        tokio::spawn(send_challenge(
            addr.protocol,
            connless_request_token_7,
            shared.socket.clone(),
            addr.to_socket_addr(),
            register.challenge_secret,
            challenge.current,
        ));
    }

    Ok(result)
}

fn handle_delete(
    shared: Shared,
    remote_addr: IpAddr,
    delete: Delete,
) -> Result<RegisterResponse, RegisterError> {
    let addr = delete.address.with_ip(remote_addr);
    match shared.lock_servers().remove(addr, delete.secret) {
        RemoveResult::Removed => {
            debug!("successfully removed {}", addr);
            Ok(RegisterResponse::Success)
        }
        RemoveResult::NotFound => Err("could not find registered server".into()),
    }
}

fn parse_opt<T: str::FromStr>(
    headers: &warp::http::HeaderMap,
    name: &str,
) -> Result<Option<T>, RegisterError>
where
    T::Err: fmt::Display,
{
    headers
        .get(name)
        .map(|v| -> Result<T, RegisterError> {
            v.to_str()
                .map_err(|e| RegisterError::new(format!("invalid header {}: {}", name, e)))?
                .parse()
                .map_err(|e| RegisterError::new(format!("invalid header {}: {}", name, e)))
        })
        .transpose()
}
fn parse<T: str::FromStr>(headers: &warp::http::HeaderMap, name: &str) -> Result<T, RegisterError>
where
    T::Err: fmt::Display,
{
    parse_opt(headers, name)?
        .ok_or_else(|| RegisterError::new(format!("missing required header {}", name)))
}

fn register_from_headers(
    headers: &warp::http::HeaderMap,
    info: &[u8],
) -> Result<Register, RegisterError> {
    Ok(Register {
        address: parse(headers, "Address")?,
        secret: parse(headers, "Secret")?,
        connless_request_token: parse_opt(headers, "Connless-Token")?,
        challenge_secret: parse(headers, "Challenge-Secret")?,
        challenge_token: parse_opt(headers, "Challenge-Token")?,
        community_token: parse_opt(headers, "Community-Token")?,
        info_serial: parse(headers, "Info-Serial")?,
        info: if !info.is_empty() {
            match headers
                .typed_get::<headers::ContentType>()
                .map(mime::Mime::from)
            {
                Some(mime) if mime.essence_str() == mime::APPLICATION_JSON => {}
                _ => return Err(RegisterError::unsupported_media_type()),
            }
            Some(json::from_slice(info).map_err(|e| {
                RegisterError::new(format!("Request body deserialize error: {}", e))
            })?)
        } else {
            None
        },
    })
}

fn delete_from_headers(headers: &warp::http::HeaderMap) -> Result<Delete, RegisterError> {
    Ok(Delete {
        address: parse(headers, "Address")?,
        secret: parse(headers, "Secret")?,
    })
}

async fn recover(err: warp::Rejection) -> Result<impl warp::Reply, warp::Rejection> {
    use warp::http::StatusCode;
    let (e, status): (&dyn fmt::Display, _) = if err.is_not_found() {
        (&"Not found", StatusCode::NOT_FOUND)
    } else if let Some(e) = err.find::<warp::reject::MethodNotAllowed>() {
        (e, StatusCode::METHOD_NOT_ALLOWED)
    } else if let Some(e) = err.find::<warp::reject::InvalidHeader>() {
        (e, StatusCode::BAD_REQUEST)
    } else if let Some(e) = err.find::<warp::reject::MissingHeader>() {
        (e, StatusCode::BAD_REQUEST)
    } else if let Some(e) = err.find::<warp::reject::InvalidQuery>() {
        (e, StatusCode::BAD_REQUEST)
    } else if let Some(e) = err.find::<warp::filters::body::BodyDeserializeError>() {
        (e, StatusCode::BAD_REQUEST)
    } else if let Some(e) = err.find::<warp::reject::LengthRequired>() {
        (e, StatusCode::LENGTH_REQUIRED)
    } else if let Some(e) = err.find::<warp::reject::PayloadTooLarge>() {
        (e, StatusCode::PAYLOAD_TOO_LARGE)
    } else {
        return Err(err);
    };

    let response = RegisterResponse::Error(RegisterError::new(format!("{}", e)));
    Ok(warp::http::Response::builder()
        .status(status)
        .header(warp::http::header::CONTENT_TYPE, "application/json")
        .body(json::to_string(&response).unwrap() + "\n"))
}

#[derive(Clone)]
struct AssertUnwindSafe<T>(pub T);
impl<T> panic::UnwindSafe for AssertUnwindSafe<T> {}
impl<T> panic::RefUnwindSafe for AssertUnwindSafe<T> {}

macro_rules! fatal {
    ($($arg:tt)*) => {
        {
            eprintln!($($arg)*);
            process::exit(1);
        }
    }
}

// TODO: put active part masterservers on a different domain?
#[tokio::main]
async fn main() {
    env_logger::init();

    let mut command = App::new("mastersrv")
        .about("Collects game server info via an HTTP endpoint and aggregates them.")
        .arg(Arg::with_name("listen")
            .long("listen")
            .value_name("ADDRESS")
            .default_value("[::]:8080")
            .help("Listen address for the HTTP endpoint.")
        )
        .arg(Arg::with_name("connecting-ip-header")
            .long("connecting-ip-header")
            .value_name("HEADER")
            .help("HTTP header to use to determine the client IP address.")
        )
        .arg(Arg::with_name("locations")
            .long("locations")
            .value_name("LOCATIONS")
            .help("IP to continent locations database filename (libloc format, can be obtained from https://location.ipfire.org/databases/1/location.db.xz).")
            .conflicts_with("config")
        )
        .arg(Arg::with_name("config")
            .long("config")
            .value_name("CONFIG")
            .help("TOML config (can be re-read using SIGHUP signal)")
        )
        .arg(Arg::with_name("write-addresses")
            .long("write-addresses")
            .value_name("FILENAME")
            .help("Dump all new-style addresses to a file each second.")
        )
        .arg(Arg::with_name("write-dump")
            .long("write-dump")
            .value_name("DUMP")
            .help("Dump the internal state to a JSON file each second.")
        )
        .arg(Arg::with_name("read-write-dump")
            .long("read-write-dump")
            .value_name("DUMP")
            .conflicts_with("write-dump")
            .help("Dump the internal state to a JSON file each second, but also read it at the start.")
        )
        .arg(Arg::with_name("read-dump-dir")
            .long("read-dump-dir")
            .takes_value(true)
            .value_name("DUMP_DIR")
            .help("Read dumps from other mastersrv instances from the specified directory (looking only at *.json files).")
        )
        .arg(Arg::with_name("test-servers-route")
            .long("test-servers-route")
            .help("Enable /ddnet/15/test-servers.json route for testing")
        )
        .arg(Arg::with_name("websockets")
            .long("websockets")
            .help("Accept registrations for the websocket protocols (ddnet-20+ws, ddnet-20+wss). Only enable this once every mastersrv instance sharing dumps has been upgraded to a version that knows these protocols.")
        )
        .arg(Arg::with_name("out")
            .long("out")
            .value_name("OUT")
            .default_value("servers.json")
            .help("Output file for the aggregated server list in a DDNet 15.5+ compatible format.")
        );

    if cfg!(unix) {
        command = command.arg(
            Arg::with_name("listen-unix")
                .long("listen-unix")
                .value_name("PATH")
                .requires("connecting-ip-header")
                .conflicts_with("listen")
                .help("Listen on the specified Unix domain socket."),
        );
    }

    let matches = command.get_matches();

    let listen_address = value_t_or_exit!(matches.value_of("listen"), SocketAddr);
    let connecting_ip_header = matches
        .value_of("connecting-ip-header")
        .map(|s| s.to_owned());
    let listen_unix = if cfg!(unix) {
        matches.value_of("listen-unix")
    } else {
        None
    };
    let read_dump_dir = matches.value_of("read-dump-dir").map(|s| s.to_owned());
    let read_write_dump = matches.value_of("read-write-dump").map(|s| s.to_owned());
    let test_servers_route = matches.is_present("test-servers-route");
    let accept_websocket_protocols = matches.is_present("websockets");
    if accept_websocket_protocols {
        // Load the TLS root certificates before serving, so that problems
        // with the certificate store surface at startup and the blocking file
        // I/O stays out of the async challenge path.
        let _ = websocket_tls_connector();
    }
    let config_filename = matches.value_of("config");

    let config_location = match (config_filename, matches.value_of("locations")) {
        (None, None) => ConfigLocation::None,
        (None, Some(l)) => ConfigLocation::LocationsFileParameter(l.into()),
        (Some(f), None) => ConfigLocation::File(f.into()),
        (Some(_), Some(_)) => unreachable!(),
    };
    let config = Arc::new(ArcSwap::from_pointee(
        config_location.read().unwrap_or_else(|err| {
            fatal!("couldn't read config: {}", err);
        }),
    ));
    let timekeeper = Timekeeper::new();
    let challenger = Arc::new(Mutex::new(Challenger::new()));
    let mut servers = Servers::new();
    match &read_write_dump {
        Some(path) => match read_dump(Path::new(&path), timekeeper).await {
            Ok(dump) => match Servers::from_dump(dump) {
                Ok(read_servers) => {
                    info!("successfully read previous dump");
                    servers = read_servers;
                }
                Err(FromDumpError) => error!("previous dump was inconsistent"),
            },
            Err(e) => error!("couldn't read previous dump: {}", e),
        },
        None => {}
    }
    let servers = Arc::new(Mutex::new(servers));
    let socket = Arc::new(tokio::net::UdpSocket::bind("[::]:0").await.unwrap());
    let socket = AssertUnwindSafe(socket);

    let task_reseed = tokio::spawn(handle_periodic_reseed(challenger.clone()));
    let task_writeout = tokio::spawn(handle_periodic_writeout(
        config.clone(),
        servers.clone(),
        read_dump_dir.clone(),
        matches
            .value_of("write-dump")
            .map(|s| s.to_owned())
            .or(read_write_dump),
        matches.value_of("write-addresses").map(|s| s.to_owned()),
        matches.value_of("out").unwrap().to_owned(),
        timekeeper,
    ));
    let task_reread = tokio::spawn(handle_config_reread(config_location, config.clone()));

    let connecting_addr = move |addr: Option<SocketAddr>,
                                headers: &warp::http::HeaderMap|
          -> Result<IpAddr, RegisterError> {
        let mut addr = if let Some(header) = &connecting_ip_header {
            headers
                .get(header)
                .ok_or_else(|| RegisterError::new(format!("missing {} header", header)))?
                .to_str()
                .map_err(|_| RegisterError::from("non-ASCII in connecting IP header"))?
                .parse()
                .map_err(|e| RegisterError::new(format!("{}", e)))?
        } else {
            addr.unwrap().ip()
        };
        if let IpAddr::V6(v6) = addr {
            if let Some(v4) = v6.to_ipv4_mapped() {
                addr = IpAddr::from(v4);
            }
        }
        Ok(addr)
    };

    fn build_response<F>(f: F) -> Result<warp::http::Response<String>, warp::http::Error>
    where
        F: FnOnce() -> Result<RegisterResponse, RegisterError> + UnwindSafe,
    {
        let (http_status, body) = match panic::catch_unwind(f) {
            Ok(Ok(r)) => (warp::http::StatusCode::OK, r),
            Ok(Err(e)) => (e.status(), RegisterResponse::Error(e)),
            Err(_) => (
                warp::http::StatusCode::INTERNAL_SERVER_ERROR,
                RegisterResponse::Error("unexpected panic".into()),
            ),
        };
        warp::http::Response::builder()
            .status(http_status)
            .header(warp::http::header::CONTENT_TYPE, "application/json")
            .body(json::to_string(&body).unwrap() + "\n")
    }

    let register = warp::path!("ddnet" / "15" / "register")
        .and(warp::post())
        .and(warp::header::headers_cloned())
        .and(warp::addr::remote())
        .and(warp::body::content_length_limit(32 * 1024)) // limit body size to 32 KiB
        .and(warp::body::bytes())
        .map({
            let config = config.clone();
            let servers = servers.clone();
            move |headers: warp::http::HeaderMap, addr: Option<SocketAddr>, info: bytes::Bytes| {
                build_response(|| {
                    let config = config.load();
                    let shared = Shared {
                        challenger: &challenger,
                        config: &config,
                        servers: &servers,
                        socket: &socket.0,
                        timekeeper,
                        accept_websocket_protocols,
                    };
                    let addr = connecting_addr(addr, &headers)?;
                    match headers.get("Action").map(warp::http::HeaderValue::as_bytes) {
                        None => {
                            let register = register_from_headers(&headers, &info)?;
                            handle_register(shared, addr, register)
                        }
                        Some(b"delete") => {
                            let delete = delete_from_headers(&headers)?;
                            handle_delete(shared, addr, delete)
                        }
                        Some(action) => {
                            Err(RegisterError::new(format!("Unknown Action header value {:?} (must either not be present or have value \"delete\"", String::from_utf8_lossy(action))))
                        }
                    }
                })
            }
        });
    let test_servers = warp::path!("ddnet" / "15" / "test-servers.json")
        .and(warp::get())
        .and_then({
            let config = config.clone();
            let servers = servers.clone();
            let read_dump_dir = read_dump_dir.clone();
            move || {
                let config = config.clone();
                let servers = servers.clone();
                let read_dump_dir = read_dump_dir.clone();
                async move {
                    if test_servers_route {
                        Ok(handle_test_servers_json(config.load_full(), servers, read_dump_dir, timekeeper).await)
                    } else {
                        Err(warp::reject())
                    }
                }
            }
        });
    let server = warp::serve(register.or(test_servers).recover(recover));

    let task_server = if let Some(path) = listen_unix {
        #[cfg(unix)]
        {
            use tokio::net::UnixListener;
            use tokio_stream::wrappers::UnixListenerStream;
            let _ = fs::remove_file(path).await;
            let unix_socket = UnixListener::bind(path).unwrap();
            tokio::spawn(server.run_incoming(UnixListenerStream::new(unix_socket)))
        }
        #[cfg(not(unix))]
        {
            let _ = path;
            unreachable!();
        }
    } else {
        tokio::spawn(server.run(listen_address))
    };

    match tokio::try_join!(task_reseed, task_writeout, task_reread, task_server) {
        Ok(((), (), (), ())) => unreachable!(),
        Err(e) => panic::resume_unwind(e.into_panic()),
    }
}

#[cfg(test)]
mod test {
    use super::Dump;
    use super::Servers;
    use super::WebsocketChallengeIpPermit;
    use super::WEBSOCKET_CHALLENGE_LIMIT_PER_IP;
    use serde_json as json;
    use std::net::IpAddr;
    use std::str::FromStr as _;

    #[test]
    fn websocket_challenge_ip_permits() {
        let limit = WEBSOCKET_CHALLENGE_LIMIT_PER_IP as usize;
        fn ip(s: &str) -> IpAddr {
            IpAddr::from_str(s).unwrap()
        }
        {
            // Addresses of the same /64 network share their permits.
            let mut permits: Vec<_> = (0..limit)
                .map(|i| {
                    WebsocketChallengeIpPermit::try_acquire(ip(&format!("2001:db8::{}", i)))
                        .unwrap()
                })
                .collect();
            assert!(WebsocketChallengeIpPermit::try_acquire(ip("2001:db8::ffff")).is_none());
            // Other networks and IPv4 addresses are unaffected.
            assert!(WebsocketChallengeIpPermit::try_acquire(ip("2001:db8:0:1::1")).is_some());
            assert!(WebsocketChallengeIpPermit::try_acquire(ip("127.0.0.1")).is_some());
            // Permits are released again.
            permits.pop();
            assert!(WebsocketChallengeIpPermit::try_acquire(ip("2001:db8::ffff")).is_some());
        }
        assert!(super::websocket_challenges_per_ip().is_empty());
    }

    #[test]
    fn dump_skips_unknown_protocols() {
        let dump: Dump = json::from_str(
            r#"{
                "now": 0,
                "addresses": {
                    "tw-0.6+udp://127.0.0.1:8303": {"kind": "mastersrv", "ping_time": 0, "secret": "s"},
                    "ddnet-99+quic://127.0.0.1:8304": {"kind": "mastersrv", "ping_time": 0, "secret": "s"}
                },
                "servers": {"s": {"info_serial": 0, "info": {}}}
            }"#,
        )
        .unwrap();
        assert_eq!(dump.addresses.len(), 1);
        assert!(dump.addresses.keys().all(|addr| addr.port == 8303));
    }

    #[test]
    fn from_dump_drops_servers_without_addresses() {
        let dump: Dump = json::from_str(
            r#"{
                "now": 0,
                "addresses": {
                    "tw-0.6+udp://127.0.0.1:8303": {"kind": "mastersrv", "ping_time": 0, "secret": "s"},
                    "ddnet-99+quic://127.0.0.1:8304": {"kind": "mastersrv", "ping_time": 0, "secret": "t"}
                },
                "servers": {
                    "s": {"info_serial": 0, "info": {}},
                    "t": {"info_serial": 0, "info": {}}
                }
            }"#,
        )
        .unwrap();
        let servers = Servers::from_dump(dump).unwrap();
        assert_eq!(servers.servers.len(), 1);
        assert!(servers.servers.keys().all(|secret| &secret[..] == "s"));
    }
}
