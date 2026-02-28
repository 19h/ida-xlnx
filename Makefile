.PHONY: all release debug loader test clean purge

all: release

.configure-release:
	cmake --preset release-optimized
	@touch .configure-release

.configure-debug:
	cmake --preset debug
	@touch .configure-debug

release: .configure-release
	cmake --build --preset release-optimized

debug: .configure-debug
	cmake --build --preset debug

loader: .configure-release
	cmake --build --preset release-optimized --target zynqmp_boot_image_loader

loader-debug: .configure-debug
	cmake --build --preset debug --target zynqmp_boot_image_loader

test: .configure-release
	cmake --build --preset release-optimized --target xilinx_loader_tests
	ctest --preset release-optimized

test-debug: .configure-debug
	cmake --build --preset debug --target xilinx_loader_tests
	ctest --preset debug

clean:
	@echo "Cleaning release artifacts..."
	@if [ -d build-release-optimized ]; then cmake --build --preset release-optimized --target clean 2>/dev/null || true; fi
	@echo "Cleaning debug artifacts..."
	@if [ -d build-debug ]; then cmake --build --preset debug --target clean 2>/dev/null || true; fi

purge:
	@echo "Purging all build directories and CMake configurations..."
	rm -rf build-release-optimized build-debug .configure-release .configure-debug
