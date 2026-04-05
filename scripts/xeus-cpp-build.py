import argparse
import os
import platform
import subprocess
from pathlib import Path
from selenium import webdriver
from selenium.common.exceptions import WebDriverException


def is_safari_driver_enabled():
    try:
        driver = webdriver.Safari()
        driver.quit()
        return True
    except WebDriverException:
        return False


def run_browser_tests_linux(build_dir, emrun_path):
    browser_install_cmd = (
        "wget https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb && "
        "dpkg-deb -x google-chrome-stable_current_amd64.deb $PWD/chrome && "
        "wget https://ftp.mozilla.org/pub/firefox/releases/138.0.1/linux-x86_64/en-GB/firefox-138.0.1.tar.xz && "
        "tar -xJf firefox-138.0.1.tar.xz"
    )
    subprocess.run(browser_install_cmd, cwd=build_dir / "test", shell=True)
    browsers = {"Firefox": "firefox", "Google Chrome": "google-chrome"}
    for name, browser in browsers.items():
        print(f"\nRunning headless tests in {name}")
        browser_args = "--headless"
        if "chrome" in name:
            browser_args += " --no-sandbox"

        browser_test_cmd = (
            'eval "$(micromamba shell hook --shell bash)" && '
            "micromamba activate xeus-cpp-wasm-build && "
            f'export PATH="{build_dir}/test/chrome/opt/google/chrome/:{build_dir}/test/firefox/:$PATH" && '
            f'python {emrun_path} --browser="{browser}" --kill_exit --timeout 60 --browser-args="{browser_args}" test_xeus_cpp.html'
        )

        subprocess.run(browser_test_cmd, cwd=build_dir / "test", shell=True)


def run_browser_tests_macos(repo_dir, build_dir, emrun_path):
    browser_install_cmd = (
        'wget "https://download.mozilla.org/?product=firefox-latest&os=osx&lang=en-US" -O Firefox-latest.dmg && '
        "hdiutil attach Firefox-latest.dmg && "
        "cp -r /Volumes/Firefox/Firefox.app . && "
        "hdiutil detach /Volumes/Firefox && "
        "wget https://dl.google.com/chrome/mac/stable/accept_tos%3Dhttps%253A%252F%252Fwww.google.com%252Fintl%252Fen_ph%252Fchrome%252Fterms%252F%26_and_accept_tos%3Dhttps%253A%252F%252Fpolicies.google.com%252Fterms/googlechrome.pkg && "
        "pkgutil --expand-full googlechrome.pkg google-chrome"
    )
    subprocess.run(browser_install_cmd, cwd=build_dir / "test", shell=True)
    browsers = {"Firefox": "firefox", "Google Chrome": "Google Chrome"}
    for name, browser in browsers.items():
        print(f"\nRunning Emscripten C++ tests in {name}")
        browser_args = "--headless"
        if "Chrome" in name:
            browser_args += " --no-sandbox"

        browser_test_cmd = (
            'eval "$(micromamba shell hook --shell bash)" && '
            "micromamba activate xeus-cpp-wasm-build && "
            f'export PATH="{build_dir}/test/Firefox.app/Contents/MacOS:{build_dir}/test/google-chrome/GoogleChrome.pkg/Payload/Google Chrome.app/Contents/MacOS/:$PATH" && '
            f'python {emrun_path} --browser="{browser}" --kill_exit --timeout 60 --browser-args="{browser_args}" test_xeus_cpp.html'
        )

        subprocess.run(browser_test_cmd, cwd=build_dir / "test", shell=True)

    if is_safari_driver_enabled():
        print("\nRunning Emscripten C++ tests in Safari")
        safari_cmd = (
            'eval "$(micromamba shell hook --shell bash)" && '
            "micromamba activate xeus-cpp-wasm-build && "
            f'python {emrun_path} --no_browser --kill_exit --kill_exit --timeout 60 --browser-args="--headless --no-sandbox" test_xeus_cpp.html & '
            f"python {repo_dir}/scripts/browser_tests_safari.py test_xeus_cpp.html"
        )
        subprocess.run(safari_cmd, cwd=build_dir / "test", shell=True)
    else:
        print(
            "Safari WebDriver is NOT enabled, so not running browser tests in Safari."
        )


def run_lite(repo_dir, prefix):
    subprocess.run(
        [
            "micromamba",
            "create",
            "-n",
            "xeus-lite-host",
            "jupyterlite-core=0.6",
            "jupyter_server",
            "jupyterlite-xeus",
            "-c",
            "conda-forge",
            "-y",
        ]
    )

    serve_cmd = [
        'eval "$(micromamba shell hook --shell bash)" && '
        "micromamba activate xeus-lite-host && "
        "jupyter lite serve "
        f"--XeusAddon.prefix={prefix} "
        f"--XeusAddon.mounts={prefix}/share/xeus-cpp/tagfiles:/share/xeus-cpp/tagfiles "
        f"--XeusAddon.mounts={prefix}/etc/xeus-cpp/tags.d:/etc/xeus-cpp/tags.d "
        f"--contents {repo_dir}/README.md "
        f"--contents {repo_dir}/notebooks/xeus-cpp-lite-demo.ipynb "
        f"--contents {repo_dir}/notebooks/smallpt.ipynb "
        f"--contents {repo_dir}/notebooks/images/marie.png "
        f"--contents {repo_dir}/notebooks/audio/audio.wav "
    ]
    subprocess.run(serve_cmd, cwd=repo_dir, shell=True)


def build_native(launch_lab=False):
    repo_dir = Path.cwd()

    subprocess.run(["micromamba", "create", "-f", "environment-dev.yml", "-y"])
    subprocess.run(
        [
            "micromamba",
            "install",
            "-n",
            "xeus-cpp",
            "jupyterlab",
            "-c",
            "conda-forge",
            "-y",
        ]
    )

    prefix_path = os.environ.get("MAMBA_ROOT_PREFIX")
    build_dir = repo_dir / "build"
    build_dir.mkdir(exist_ok=True)

    cmake_cmd = [
        "micromamba",
        "run",
        "-n",
        "xeus-cpp",
        "cmake",
        "..",
        f"-DCMAKE_PREFIX_PATH={prefix_path}/envs/xeus-cpp/",
        f"-DCMAKE_INSTALL_PREFIX={prefix_path}/envs/xeus-cpp/",
        "-DCMAKE_INSTALL_LIBDIR=lib",
    ]

    subprocess.run(cmake_cmd, cwd=build_dir)
    subprocess.run(
        ["micromamba", "run", "-n", "xeus-cpp", "make", "check-xeus-cpp"], cwd=build_dir
    )

    subprocess.run(["make", "install"], cwd=build_dir)
    subprocess.run(
        [f"{prefix_path}/envs/xeus-cpp/bin/pytest", "-sv", "test_xcpp_kernel.py"],
        cwd=repo_dir / "test",
    )

    if launch_lab:
        subprocess.run([f"{prefix_path}/envs/xeus-cpp/bin/jupyter-lab"])


def build_emscripten(run_browser_tests_flag=False, launch_lite=False):
    repo_dir = Path.cwd()

    subprocess.run(["micromamba", "create", "-f", "environment-wasm-build.yml", "-y"])
    subprocess.run(
        [
            "micromamba",
            "create",
            "-f",
            "environment-wasm-host.yml",
            "--platform=emscripten-wasm32",
            "-y",
        ]
    )

    mamba_root = os.environ.get("MAMBA_ROOT_PREFIX")

    build_prefix = f"{mamba_root}/envs/xeus-cpp-wasm-build"
    prefix = f"{mamba_root}/envs/xeus-cpp-wasm-host"
    sysroot_path = f"{build_prefix}/opt/emsdk/upstream/emscripten/cache/sysroot"

    subprocess.run(
        [
            "micromamba",
            "create",
            "-n",
            "node-env",
            "-c",
            "conda-forge",
            "nodejs=22",
            "-y",
        ]
    )

    build_dir = repo_dir / "build"
    build_dir.mkdir(exist_ok=True)

    cmake_cmd = (
        'eval "$(micromamba shell hook --shell bash)" && '
        "micromamba activate xeus-cpp-wasm-build && "
        f"export PATH={mamba_root}/envs/node-env/bin:$PATH &&"
        "emcmake cmake "
        f"-DCMAKE_BUILD_TYPE=Release "
        f"-DCMAKE_INSTALL_PREFIX={prefix} "
        "-DXEUS_CPP_EMSCRIPTEN_WASM_BUILD=ON "
        f"-DCMAKE_FIND_ROOT_PATH={prefix} "
        f"-DSYSROOT_PATH={sysroot_path} "
        ".."
    )
    subprocess.run(cmake_cmd, cwd=build_dir, shell=True)
    make_cmd = (
        'eval "$(micromamba shell hook --shell bash)" && '
        "micromamba activate xeus-cpp-wasm-build && "
        f"export PATH={mamba_root}/envs/node-env/bin:$PATH &&"
        "emmake make check-xeus-cpp"
    )
    subprocess.run(make_cmd, cwd=build_dir, shell=True)
    install_cmd = (
        f"export PATH={mamba_root}/node-env/bin:$PATH &&"
        'eval "$(micromamba shell hook --shell bash)" && '
        "micromamba activate xeus-cpp-wasm-build && "
        f"export PATH={mamba_root}/envs/node-env/bin:$PATH &&"
        "emmake make install"
    )
    subprocess.run(install_cmd, cwd=build_dir, shell=True)

    if run_browser_tests_flag:
        if platform.system() == "Darwin":
            run_browser_tests_macos(
                repo_dir=repo_dir,
                build_dir=repo_dir / "build",
                emrun_path=f"{build_prefix}/bin/emrun.py",
            )
        elif platform.system() == "Linux":
            run_browser_tests_linux(
                build_dir=repo_dir / "build", emrun_path=f"{build_prefix}/bin/emrun.py"
            )

    if launch_lite:
        run_lite(repo_dir=repo_dir, prefix=prefix)


def main():
    parser = argparse.ArgumentParser(
        description="Build xeus-cpp (native or emscripten)."
    )
    parser.add_argument(
        "--build",
        choices=["native", "emscripten"],
        default="native",
        help="Choose build type (default: native)",
    )
    parser.add_argument(
        "--run-browser-tests",
        action="store_true",
        help="Run headless browser tests after Emscripten build",
    )
    parser.add_argument(
        "--launch-lite",
        action="store_true",
        help="Launch a local JupyterLite demo after Emscripten build",
    )
    parser.add_argument(
        "--launch-lab", action="store_true", help="Launch JupyterLab after native build"
    )
    args = parser.parse_args()

    if args.build == "emscripten":
        build_emscripten(
            run_browser_tests_flag=args.run_browser_tests, launch_lite=args.launch_lite
        )
    else:
        build_native(launch_lab=args.launch_lab)


if __name__ == "__main__":
    main()
