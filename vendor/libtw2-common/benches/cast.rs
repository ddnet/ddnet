use bencher::benchmark_group;
use bencher::benchmark_main;
use bencher::black_box;
use bencher::Bencher;
use libtw2_common::num::Cast;

fn u32_assert_u8(bench: &mut Bencher) {
    fn helper() {
        black_box(black_box(0u32).assert_u8());
    }
    bench.iter(helper);
}

fn u32_assert_u8_inline(bench: &mut Bencher) {
    fn helper() {
        let i = black_box(0u32);
        if i >= 256 {
            panic!();
        } else {
            black_box(i as u8);
        }
    }
    bench.iter(helper);
}

fn u32_try_u8(bench: &mut Bencher) {
    fn helper() {
        match black_box(0u32).try_u8() {
            Some(x) => {
                black_box(x);
            }
            None => {}
        }
    }
    bench.iter(helper);
}

fn u32_try_u8_inline(bench: &mut Bencher) {
    fn helper() {
        let i = black_box(0u32);
        if i < 256 {
            black_box(i);
        }
    }
    bench.iter(helper);
}

fn u8_u32(bench: &mut Bencher) {
    fn helper() {
        black_box(black_box(0u8).u32());
    }
    bench.iter(helper);
}

fn u8_u32_inline(bench: &mut Bencher) {
    fn helper() {
        black_box(black_box(0u8) as u32);
    }
    bench.iter(helper);
}

benchmark_group!(
    cast,
    u32_assert_u8,
    u32_assert_u8_inline,
    u32_try_u8,
    u32_try_u8_inline,
    u8_u32,
    u8_u32_inline,
);
benchmark_main!(cast);
