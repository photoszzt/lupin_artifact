# Repository Guidelines

## Project Structure & Module Organization
`driver/` builds the CXL-aware kernel allocator module (`cxl_ivpci.ko`). Shared allocators and low-level headers live under `src/common*` and feed both the driver and benchmarking code. `lupin_lock_benchmark/` contains standalone CMake targets that stress locks on DRAM and CXL hardware. The `ivshmem/` suite mixes a Linux kernel module, Rust host utilities, and Rust benchmarking front ends. Python automation in `vm_lib/` provisions VMs and manages PCI passthrough.

## Build, Test, and Development Commands
- `make -C driver` / `make -C driver clean` build or reset the kernel module against the active kernel headers.
- `cmake -S lupin_lock_benchmark -B build && cmake --build build -j$(nproc)` generates the `dram_test_*` binaries; run `./build/dram_test_rmcs` (etc.) to gather latency numbers.
- `cargo build --release` inside `ivshmem/ivshmem-host` and `ivshmem/ivshmem-user` emits the Rust artifacts under `target/release`.
- `python -m vm_lib.start_vm --help` shows VM runtime options, while `sudo python -m vm_lib.test_ivshmem` performs a loopback IVSHMEM smoke test that needs `/mnt/cxl_mem`.

## Coding Style & Naming Conventions
Driver C code follows the Linux `.clang-format` (tabs, 80 columns, K&R braces); re-run `clang-format` before sending patches. Benchmarks share the brace style and prefer lower_snake_case helpers such as `measure_rmcs`. Rust crates target Edition 2021—use `cargo fmt`/`cargo clippy` to stay consistent. Python modules mirror PEP 8 with explicit type hints (`VMConfig`, `setup_bridge_tap_network`) and snake_case filenames.

## Testing Guidelines
Benchmarks double as regression tests; name new experiments `dram_test_<lock>` or `cxl_test_<lock>` and include their stdout in reviews. Always reload the kernel module (`sudo insmod driver/cxl_ivpci.ko && dmesg | tail`) after touching `driver/` or `src/common`. Run `sudo python -m vm_lib.test_ivshmem` whenever IVSHMEM paths, networking, or VM orchestration changes. Even when no unit tests exist, `cargo test` in each Rust crate must pass to catch doc-test regressions.

## Commit & Pull Request Guidelines
Keep commit subjects short, imperative, and prefixed with the subsystem, e.g., `driver: guard proc_death path` or `vm_lib: add NUMA flag`. Include a short rationale and a `Tested:` trailer that lists the commands above. Pull requests should link issues, summarize design decisions, and attach relevant artifacts (benchmark output, `dmesg`, VM console logs) so reviewers can reproduce the scenario.

## Security & Configuration Tips
Kernel and IVSHMEM binaries require elevated privileges—never ship hard-coded device paths, MACs, or credentials. Scripts in `vm_lib/` touch iptables and TAP devices, so confirm parameters before running on shared hosts. Keep scratch directories such as `/mnt/cxl_mem` on local storage and scrub sensitive data from uploaded logs.
