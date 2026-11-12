# Build RAKT on an Apple Silicon Mac

Start Docker Desktop. The build uses the JieLi toolchain in a Linux x86-64 container.

```sh
python3 -m venv ../work/venv
../work/venv/bin/pip install Pillow==11.1.0 fonttools==4.66.1
sh tools/get_toolchain.sh ../work/jieli
PYTHON="$PWD/../work/venv/bin/python" \
JIELI_TOOLCHAIN="$PWD/../work/jieli/toolchain" \
./build.sh
```

The installable package is `build/rakt.fwsc`. Add `--release VERSION` to write a versioned package, application image, checksums and licenses to `build/release-VERSION/`.
