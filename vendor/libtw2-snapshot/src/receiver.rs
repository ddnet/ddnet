use crate::to_usize;
use libtw2_common::num::Cast;
use libtw2_gamenet_snap as msg;
use libtw2_warn::Warn;
use std::ops;
use vec_map::VecMap;

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Warning {
    DuplicateSnap,
    DifferingAttributes,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Error {
    OldDelta,
    InvalidNumParts,
    InvalidPart,
    DuplicatePart,
}

// TODO: How to handle `tick` overflowing?
#[derive(Clone, Debug)]
struct CurrentDelta {
    tick: i32,
    delta_tick: i32,
    num_parts: i32,
    crc: i32,
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct ReceivedDelta<'a> {
    pub delta_tick: i32,
    pub tick: i32,
    pub data_and_crc: Option<(&'a [u8], i32)>,
}

#[derive(Clone, Default)]
pub struct DeltaReceiver {
    previous_tick: Option<i32>,
    current: Option<CurrentDelta>,
    // `parts` points into `receive_buf`.
    parts: VecMap<ops::Range<u32>>,
    receive_buf: Vec<u8>,
    result: Vec<u8>,
}

impl DeltaReceiver {
    pub fn new() -> DeltaReceiver {
        Default::default()
    }
    pub fn reset(&mut self) {
        self.previous_tick = None;
        self.current = None;
    }
    fn can_receive(&self, tick: i32) -> bool {
        self.current
            .as_ref()
            .map(|c| c.tick <= tick)
            .or(self.previous_tick.map(|t| t < tick))
            .unwrap_or(true)
    }
    fn init_delta(&mut self) {
        self.parts.clear();
        self.receive_buf.clear();
        self.result.clear();
    }
    fn finish_delta(&mut self, tick: i32) {
        self.current = None;
        self.previous_tick = Some(tick);
    }
    pub fn snap_empty<W: Warn<Warning>>(
        &mut self,
        warn: &mut W,
        snap: msg::SnapEmpty,
    ) -> Result<Option<ReceivedDelta<'_>>, Error> {
        if !self.can_receive(snap.tick) {
            return Err(Error::OldDelta);
        }
        if self
            .current
            .as_ref()
            .map(|c| c.tick == snap.tick)
            .unwrap_or(false)
        {
            warn.warn(Warning::DuplicateSnap);
        }
        self.init_delta();
        self.finish_delta(snap.tick);
        Ok(Some(ReceivedDelta {
            delta_tick: snap.tick.wrapping_sub(snap.delta_tick),
            tick: snap.tick,
            data_and_crc: None,
        }))
    }
    pub fn snap_single<W: Warn<Warning>>(
        &mut self,
        warn: &mut W,
        snap: msg::SnapSingle,
    ) -> Result<Option<ReceivedDelta<'_>>, Error> {
        if !self.can_receive(snap.tick) {
            return Err(Error::OldDelta);
        }
        if self
            .current
            .as_ref()
            .map(|c| c.tick == snap.tick)
            .unwrap_or(false)
        {
            warn.warn(Warning::DuplicateSnap);
        }
        self.init_delta();
        self.finish_delta(snap.tick);
        self.result.extend(snap.data);
        Ok(Some(ReceivedDelta {
            delta_tick: snap.tick.wrapping_sub(snap.delta_tick),
            tick: snap.tick,
            data_and_crc: Some((&self.result, snap.crc)),
        }))
    }
    pub fn snap<W: Warn<Warning>>(
        &mut self,
        warn: &mut W,
        snap: msg::Snap,
    ) -> Result<Option<ReceivedDelta<'_>>, Error> {
        if !self.can_receive(snap.tick) {
            return Err(Error::OldDelta);
        }
        if !(0 <= snap.num_parts && snap.num_parts <= 32) {
            return Err(Error::InvalidNumParts);
        }
        if !(0 <= snap.part && snap.part < snap.num_parts) {
            return Err(Error::InvalidPart);
        }
        if self
            .current
            .as_ref()
            .map(|c| c.tick != snap.tick)
            .unwrap_or(false)
        {
            self.current = None;
        }
        if let None = self.current {
            self.init_delta();
            self.current = Some(CurrentDelta {
                tick: snap.tick,
                delta_tick: snap.tick.wrapping_sub(snap.delta_tick),
                num_parts: snap.num_parts,
                crc: snap.crc,
            });

            // Checked above.
            let num_parts = snap.num_parts.assert_usize();
            self.parts.reserve_len(num_parts);
        }
        let delta_tick;
        let tick;
        let crc;
        let num_parts;
        {
            let current: &mut CurrentDelta = self.current.as_mut().unwrap();
            if snap.delta_tick != current.delta_tick
                || snap.num_parts != current.num_parts
                || snap.crc != current.crc
            {
                warn.warn(Warning::DifferingAttributes);
            }
            delta_tick = current.delta_tick;
            tick = current.tick;
            crc = current.crc;
            num_parts = current.num_parts;
        }
        let part = snap.part.assert_usize();
        if self.parts.contains_key(part) {
            return Err(Error::DuplicatePart);
        }
        let start = self.receive_buf.len().assert_u32();
        let end = (self.receive_buf.len() + snap.data.len()).assert_u32();
        self.receive_buf.extend(snap.data);
        assert!(self.parts.insert(part, start..end).is_none());

        if self.parts.len().assert_i32() != num_parts {
            return Ok(None);
        }

        self.finish_delta(tick);
        self.result.reserve(self.receive_buf.len());
        for range in self.parts.values() {
            self.result
                .extend(&self.receive_buf[to_usize(range.clone())]);
        }

        Ok(Some(ReceivedDelta {
            delta_tick: delta_tick,
            tick: tick,
            data_and_crc: Some((&self.result, crc)),
        }))
    }
}

#[cfg(test)]
mod test {
    use super::DeltaReceiver;
    use super::Error;
    use super::ReceivedDelta;
    use libtw2_common::num::Cast;
    use libtw2_gamenet_snap::Snap;
    use libtw2_gamenet_snap::SnapEmpty;
    use libtw2_gamenet_snap::SnapSingle;
    use libtw2_warn::Panic;

    #[test]
    fn old() {
        let mut receiver = DeltaReceiver::new();
        {
            let result = receiver
                .snap_empty(
                    &mut Panic,
                    SnapEmpty {
                        tick: 1,
                        delta_tick: 2,
                    },
                )
                .unwrap();

            assert_eq!(
                result,
                Some(ReceivedDelta {
                    delta_tick: -1,
                    tick: 1,
                    data_and_crc: None,
                })
            );
        }

        assert_eq!(
            receiver
                .snap_single(
                    &mut Panic,
                    SnapSingle {
                        tick: 0,
                        delta_tick: 0,
                        data: b"123",
                        crc: 0,
                    }
                )
                .unwrap_err(),
            Error::OldDelta
        );
    }

    #[test]
    fn reorder() {
        let mut receiver = DeltaReceiver::new();
        let chunks: &[(i32, &[u8])] = &[(3, b"3"), (2, b"2"), (4, b"4_"), (1, b"1__"), (0, b"0")];
        for &(i, c) in chunks {
            let result = receiver
                .snap(
                    &mut Panic,
                    Snap {
                        tick: 2,
                        delta_tick: 1,
                        num_parts: chunks.len().assert_i32(),
                        part: i,
                        crc: 3,
                        data: c,
                    },
                )
                .unwrap();
            if i != 0 {
                assert_eq!(result, None);
            } else {
                assert_eq!(
                    result,
                    Some(ReceivedDelta {
                        delta_tick: 1,
                        tick: 2,
                        data_and_crc: Some((b"01__234_", 3)),
                    })
                );
            }
        }
    }
}
