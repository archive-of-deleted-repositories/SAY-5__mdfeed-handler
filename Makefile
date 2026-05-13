.PHONY: build test bench fuzz lint asan clean format

BUILD ?= build

build:
	cmake -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD) -j

test: build
	cd $(BUILD) && ctest --output-on-failure

asan:
	cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DMDFEED_ASAN=ON \
	    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
	cmake --build build-asan -j
	cd build-asan && ctest --output-on-failure

bench: build
	mkdir -p bench/results
	$(BUILD)/loopback_bench --count=200000 --rate=50000 --out=bench/results/bench_local.json
	@cat bench/results/bench_local.json

fuzz:
	cmake -S . -B build-fuzz -DCMAKE_BUILD_TYPE=Debug -DMDFEED_BUILD_FUZZ=ON \
	    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
	cmake --build build-fuzz -j

fuzz-smoke: fuzz
	build-fuzz/normalize_a_fuzz -runs=5000
	build-fuzz/normalize_b_fuzz -runs=5000

format:
	clang-format -i $(shell find src tests bench -name '*.h' -o -name '*.cpp')

lint:
	clang-format --dry-run --Werror $(shell find src tests bench -name '*.h' -o -name '*.cpp')

clean:
	rm -rf build build-asan build-fuzz
