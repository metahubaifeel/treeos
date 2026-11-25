//! QA only: wall vs calling-thread CPU per Ruffle tick (Linux aarch64/x86_64).
//! No GL queries, game data or syswide counters. Timestamps use CLOCK_MONOTONIC,
//! also used by Python's time.monotonic_ns() for marking combat intervals.
use std::ffi::{c_int, c_long};
use std::fs::File;
use std::io::{BufWriter, Write};

#[repr(C)]
#[derive(Default)]
struct Timespec { sec: c_long, nsec: c_long }
unsafe extern "C" { fn clock_gettime(clock: c_int, ts: *mut Timespec) -> c_int; }
fn ns(clock: c_int) -> Option<u64> {
    let mut ts = Timespec::default();
    (unsafe { clock_gettime(clock, &mut ts) } == 0)
        .then(|| ts.sec as u64 * 1_000_000_000 + ts.nsec as u64)
}

pub struct Trace { out: BufWriter<File>, left: usize }
impl Trace {
    pub fn open() -> Option<Self> {
        // Do not create files simply because the player was started. Never overwrite.
        let path = std::env::var_os("FLASH_QA_TICK_TRACE")?;
        let file = File::create_new(path).inspect_err(|e| eprintln!("QA tick trace: {e}")).ok()?;
        let mut out = BufWriter::with_capacity(65536, file);
        writeln!(out, "end_mono_ns,frames,wall_ns,thread_cpu_ns").ok()?;
        Some(Self { out, left: 100_000 })
    }
    pub fn start(&self) -> Option<(u64, u64)> {
        if self.left == 0 { return None; }
        Some((ns(1)?, ns(3)?)) // CLOCK_MONOTONIC and CLOCK_THREAD_CPUTIME_ID
    }
    pub fn end(&mut self, start: (u64, u64), frames: u64) {
        let (Some(cpu), Some(end)) = (ns(3), ns(1)) else { return; };
        let wall = end.saturating_sub(start.0);
        if frames == 0 && wall < 2_000_000 { return; } // exclude cheap idle polls
        if writeln!(self.out, "{end},{frames},{wall},{}", cpu.saturating_sub(start.1)).is_err() {
            self.left = 0;
        } else { self.left -= 1; }
    }
    pub fn flush(&mut self) { if self.out.flush().is_err() { self.left = 0; } }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn sleep_and_busy_loop_have_distinct_thread_times() {
        let w = ns(1).unwrap(); let c = ns(3).unwrap();
        std::thread::sleep(std::time::Duration::from_millis(30));
        assert!(ns(1).unwrap() - w >= 30_000_000);
        assert!(ns(3).unwrap() - c < 15_000_000);
        let c = ns(3).unwrap();
        while ns(3).unwrap() - c < 10_000_000 { std::hint::spin_loop(); }
        assert!(ns(1).unwrap() - w >= 40_000_000);
    }
}
