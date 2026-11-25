//! QA-only Cargo bin for the pinned ruffle4consoles tree. No SDL/window,
//! native audio, network, user storage, or VSync. Reports timeline update cost,
//! NOT displayed FPS. stdout is reserved for comparing original SWF traces.
use ruffle_core::{PlayerBuilder, backend::log::LogBackend, limits::ExecutionLimit, tag_utils::SwfMovie};
use std::time::Instant;
struct Log;
impl LogBackend for Log {
    fn avm_trace(&self, message: &str) { println!("{message}"); }
    fn avm_warning(&self, message: &str) { eprintln!("AVM warning: {message}"); }
}
fn main() {
    let args: Vec<_> = std::env::args().collect();
    assert_eq!(args.len(), 4, "usage: timeline-qa SWF WARMUP_FRAMES MEASURED_FRAMES");
    let warm: usize = args[2].parse().unwrap();
    let n: usize = args[3].parse().unwrap();
    assert!((1..=100000).contains(&n) && warm <= 100000);
    let movie = SwfMovie::from_data(&std::fs::read(&args[1]).unwrap(), "file:///qa/movie.swf".into(), None).unwrap();
    let p = PlayerBuilder::new().with_movie(movie).with_log(Log).with_autoplay(true).build();
    let mut p = p.lock().unwrap();
    assert!(p.preload(&mut ExecutionLimit::none()));
    let dt = 1000.0 / p.frame_rate();
    let mut times = Vec::with_capacity(n);
    for i in 0..warm+n {
        let start = Instant::now();
        p.run_frame(); p.update_timers(dt); p.audio_mut().tick();
        if i >= warm { times.push(start.elapsed().as_nanos()); }
    }
    times.sort_unstable();
    let pct = |q: usize| times[(q*n).div_ceil(100)-1] as f64/1e6;
    eprintln!("timeline_ms n={n} p50={:.3} p95={:.3} p99={:.3}",pct(50),pct(95),pct(99));
}
