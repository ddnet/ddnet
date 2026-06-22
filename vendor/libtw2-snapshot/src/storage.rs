use crate::format;
use crate::snap;
use crate::snap::Builder;
use crate::snap::Delta;
use crate::snap::Snap;
use libtw2_warn::wrap;
use libtw2_warn::Warn;
use std::collections::VecDeque;

// TODO: Separate server storage from client storage.
// TODO: Delete snapshots over time.

#[derive(Clone)]
struct StoredSnap {
    snap: Snap,
    tick: i32,
}

const MAX_STORED_SNAPSHOT: usize = 100;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct UnknownSnap;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct WeirdNegativeDeltaTick;

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Error {
    OldDelta,
    UnknownSnap,
    InvalidCrc,
    Unpack(snap::Error),
}

impl From<snap::Error> for Error {
    fn from(err: snap::Error) -> Error {
        Error::Unpack(err)
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Warning {
    WeirdNegativeDeltaTick,
    Unpack(format::Warning),
}

impl From<format::Warning> for Warning {
    fn from(w: format::Warning) -> Warning {
        Warning::Unpack(w)
    }
}

#[derive(Clone, Default)]
pub struct Storage {
    /// Queue that stores received snaps.
    ///
    /// The newest elements are in the front.
    snaps: VecDeque<StoredSnap>,
    free: Vec<Snap>,
    ack_tick: Option<i32>,
    delta: Delta,
    delta_tick: Option<i32>,
}

impl Storage {
    pub fn new() -> Storage {
        Default::default()
    }
    pub fn reset(&mut self) {
        let self_free = &mut self.free;
        // FIXME: Replace with something like `exhaust`.
        self.snaps.drain(..).map(|s| self_free.push(s.snap)).count();
        self.ack_tick = None;
    }
    pub fn ack_tick(&self) -> Option<i32> {
        self.ack_tick
    }
    pub fn add_delta<W>(
        &mut self,
        warn: &mut W,
        crc: Option<i32>,
        delta_tick: i32,
        tick: i32,
        delta: &Delta,
    ) -> Result<&Snap, Error>
    where
        W: Warn<Warning>,
    {
        if self.snaps.front().map(|s| s.tick).unwrap_or(-1) >= tick {
            return Err(Error::OldDelta);
        }
        {
            let empty = Snap::empty();
            let delta_snap;
            if delta_tick >= 0 {
                if let Some(i) = self.snaps.iter().position(|s| s.tick < delta_tick) {
                    let self_free = &mut self.free;
                    // FIXME: Replace with something like `exhaust`.
                    self.snaps
                        .drain(i..)
                        .map(|s| self_free.push(s.snap))
                        .count();
                }
                if let Some(d) = self.snaps.back() {
                    if d.tick == delta_tick {
                        delta_snap = &d.snap;
                    } else {
                        self.ack_tick = None;
                        return Err(Error::UnknownSnap);
                    }
                } else {
                    self.ack_tick = None;
                    return Err(Error::UnknownSnap);
                }
            } else {
                delta_snap = &empty;
                if delta_tick != -1 {
                    warn.warn(Warning::WeirdNegativeDeltaTick);
                }
            }
            if self.free.is_empty() {
                self.free.push(Snap::empty());
            }

            let new_snap: &mut Snap = self.free.last_mut().unwrap();
            new_snap.read_with_delta(wrap(warn), delta_snap, delta)?;
            if crc.map(|crc| crc != new_snap.crc()).unwrap_or(false) {
                self.ack_tick = None;
                return Err(Error::InvalidCrc);
            }
            self.ack_tick = Some(tick);
        }
        self.snaps.push_front(StoredSnap {
            tick: tick,
            snap: self.free.pop().unwrap(),
        });
        if self.snaps.len() > MAX_STORED_SNAPSHOT {
            self.free.push(self.snaps.pop_back().unwrap().snap);
        }
        Ok(&self.snaps.front().unwrap().snap)
    }
    pub fn new_builder(&mut self) -> Builder {
        self.free.pop().unwrap_or_default().recycle()
    }
    pub fn set_delta_tick<W>(&mut self, warn: &mut W, tick: i32) -> Result<(), UnknownSnap>
    where
        W: Warn<WeirdNegativeDeltaTick>,
    {
        if tick < 0 {
            if tick != -1 {
                warn.warn(WeirdNegativeDeltaTick);
            }
            self.delta_tick = None;
            return Ok(());
        }
        if let Some(i) = self.snaps.iter().position(|s| s.tick < tick) {
            let self_free = &mut self.free;
            // FIXME: Replace with something like `exhaust`.
            self.snaps
                .drain(i..)
                .map(|s| self_free.push(s.snap))
                .count();
        }
        if !self.snaps.back().map(|s| s.tick == tick).unwrap_or(false) {
            self.delta_tick = None;
            return Err(UnknownSnap);
        }
        self.delta_tick = Some(tick);
        Ok(())
    }
    pub fn delta_tick(&self) -> Option<i32> {
        self.delta_tick
    }
    pub fn add_snap(&mut self, tick: i32, snap: Snap) -> &Delta {
        self.snaps.push_front(StoredSnap {
            snap: snap,
            tick: tick,
        });
        let tmp;
        let delta_snap = if self.delta_tick.is_some() {
            &self.snaps.back().unwrap().snap
        } else {
            tmp = Snap::empty();
            &tmp
        };
        self.delta
            .create(delta_snap, &self.snaps.front().unwrap().snap);
        &self.delta
    }
}
