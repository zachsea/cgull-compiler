.PHONY: all cmake cmake-ninja build clean run ninja

all: build

cmake:
	@mkdir -p build
	@cd build && cmake ..

cmake-ninja:
	@mkdir -p build
	@cd build && cmake -G Ninja ..

build: cmake
	@cd build && if [ -f build.ninja ]; then \
		echo "Using Ninja build system"; \
		ninja; \
	else \
		echo "Using Make build system"; \
		make; \
	fi

clean:
	@rm -rf build

ninja: cmake-ninja
	@cd build && ninja
