"""Building and running the two hosts, and compiling the Apple side.

Both hosts read a text manifest and exchange raw buffer files, so everything that needs
judgement (types, inputs, comparison) stays in Python, in one place.
"""

import hashlib
import json
import os
import re
import shutil
import signal
import subprocess
import sys

import hostenv

HERE = os.path.dirname(os.path.abspath(__file__))
TIMEOUT = 120


class Unavailable(Exception):
    """A host cannot run here (no Metal device, no lavapipe). Not a finding."""


class HostError(Exception):
    """A host failed for a reason that is not a result (bad manifest, Vulkan error)."""


class Refused(HostError):
    """A host cannot do what the manifest asks on this device (a texel buffer format or feature it
    lacks): the file is skipped with the reason, never passed or failed."""


def _refusal(stderr):
    """The reason when a host's stderr says `unsupported: <reason>`, else None."""
    m = re.search(r"^(?:metalhost|vkhost): unsupported: (.*)$", stderr, re.M)
    return m.group(1) if m else None


def _built(source, name, compile_cmd):
    digest = hashlib.sha256(hostenv.read_bytes(source)).hexdigest()[:12]
    out_dir = os.path.join(os.environ.get("TMPDIR", "/tmp"), "diffcheck-hosts")
    os.makedirs(out_dir, exist_ok=True)
    binary = os.path.join(out_dir, "%s-%s" % (name, digest))
    if not os.path.exists(binary):
        subprocess.run(compile_cmd(binary), check=True)
    return binary


def vkhost(explicit=None):
    if explicit:
        return explicit
    include = next((p for p in ("/opt/homebrew/include", "/usr/local/include", "/usr/include")
                    if os.path.exists(os.path.join(p, "vulkan/vulkan.h"))), None)
    lib = next((p for p in ("/opt/homebrew/lib", "/usr/local/lib", "/usr/lib", "/usr/lib/x86_64-linux-gnu",
                            "/usr/lib/aarch64-linux-gnu") if any(f.startswith("libvulkan.") for f in _ls(p))), None)
    if include is None or lib is None:
        raise Unavailable("no Vulkan headers or loader")
    source = os.path.join(HERE, "vkhost.c")
    return _built(source, "vkhost", lambda out: [
        "cc", "-O1", "-I" + include, source, "-L" + lib, "-lvulkan", "-Wl,-rpath," + lib, "-o", out])


def metalhost(explicit=None):
    if explicit:
        return explicit
    if sys.platform != "darwin":
        raise Unavailable("Metal needs macOS")
    source = os.path.join(HERE, "metalhost.mm")
    return _built(source, "metalhost", lambda out: [
        "clang++", "-std=c++17", "-fobjc-arc", source, "-framework", "Metal", "-framework", "Foundation", "-o", out])


def _ls(path):
    try:
        return os.listdir(path)
    except OSError:
        return []


def _run(cmd, env=None):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT, env=env)
    except subprocess.TimeoutExpired:
        raise HostError("%s timed out after %ds (GPU hang?)" % (os.path.basename(cmd[0]), TIMEOUT))


def run_metal(binary, manifest_path):
    r = _run([binary, manifest_path])
    if r.returncode != 0:
        if "no Metal device" in r.stderr:
            raise Unavailable("no Metal device")
        if "Apple compile failed" in r.stderr or "pipeline failed" in r.stderr:
            raise HostError("apple: " + r.stderr.strip().splitlines()[0])
        if _refusal(r.stderr):
            raise Refused("unsupported: Apple: " + _refusal(r.stderr))
        raise HostError(r.stderr.strip() or "metalhost exit %d" % r.returncode)
    return r.stdout


def run_vulkan(binary, manifest_path):
    env = hostenv.vulkan_env()
    if env is None:
        raise Unavailable("no lavapipe ICD")
    r = _run([binary, manifest_path], env=env)
    if r.returncode != 0:
        if "no Vulkan device" in r.stderr:
            raise Unavailable("no Vulkan device")
        if _refusal(r.stderr):
            raise Refused("unsupported: lavapipe: " + _refusal(r.stderr))
        raise HostError("vkhost: " + (r.stderr.strip() or "exit %d" % r.returncode))
    if "llvmpipe" not in r.stdout:
        raise HostError("vkhost ran on %r, not lavapipe" % r.stdout.strip())
    return r.stdout


def run_mslc(mslc, source, work, includes, stage="kernel"):
    """(spv path, reflection, None) or (None, None, "CRASH|INVALID <reason>" or a compile error)."""
    spv, refl = os.path.join(work, "mslc-%s.spv" % stage), os.path.join(work, "mslc-%s.json" % stage)
    cmd = [mslc, "--stage", stage, "-o", spv, "--reflect", refl, source] + ["-I" + i for i in includes]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT)
    if r.returncode < 0:
        return None, None, "CRASH mslc died with signal %d (%s)" % (-r.returncode, signal.Signals(-r.returncode).name)
    if r.returncode != 0:
        return None, None, (r.stderr.strip().splitlines() or ["mslc failed"])[-1]
    val = subprocess.run([shutil.which("spirv-val") or "spirv-val", "--target-env", "vulkan1.3", spv],
                         capture_output=True, text=True)
    if val.returncode != 0:
        return None, None, "INVALID mslc output fails spirv-val: " + (val.stdout + val.stderr).strip().splitlines()[0]
    return spv, hostenv.read_json(refl), None


def mslc_failure(error):
    """(status, reason) for a run_mslc error: a crash is an error and invalid output is a
    finding; only a compile error is a skip."""
    for prefix, status in (("INVALID ", "invalid"), ("CRASH ", "error")):
        if error.startswith(prefix):
            return status, error[len(prefix):]
    return "skipped", error


def apple_source_lines(source, work, includes, std, contract=False):
    """Manifest lines that make the Apple side compile `source`.

    newLibraryWithSource with MTLMathModeSafe, the way an app compiles, with fp contraction
    off: Metal contracts a * b + c into one fma even in safe math mode, mslc emits a
    separate multiply and add, and the two differ by a ULP or more under cancellation,
    which is a permitted difference and not a miscompile. `contract` keeps it on.
    A quoted #include or __has_include("...") cannot be resolved from an in-memory source, so such a file is built
    with `xcrun metal` (same compiler, same flags) into a metallib instead.
    """
    text = hostenv.read_text(source)
    if not re.search(r'^\s*#\s*include\s*"|__has_include\s*\(\s*"', text, re.M):
        return ["source " + source, "contract " + ("on" if contract else "off")] + (["std " + std] if std else [])
    air, lib = os.path.join(work, "apple.air"), os.path.join(work, "apple.metallib")
    cmd = ["xcrun", "metal", "-c", "-fmetal-math-mode=safe", "-ffp-contract=" + ("on" if contract else "off"),
           "-o", air, source] + ["-I" + i for i in includes] + (["-std=metal" + std] if std else [])
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT)
        if r.returncode == 0:
            r = subprocess.run(["xcrun", "metallib", air, "-o", lib], capture_output=True, text=True, timeout=TIMEOUT)
    except FileNotFoundError:
        raise Unavailable("xcrun metal not found (needed for a quoted #include)")
    if r.returncode != 0:
        raise HostError("apple: " + (r.stderr.strip().splitlines() or ["xcrun metal failed"])[0])
    return ["metallib " + lib]


def apple_reflection(binary, source_lines, entry, work):
    out = os.path.join(work, "apple-reflection.json")
    path = os.path.join(work, "apple-reflect.txt")
    hostenv.write_text(path, "\n".join(["mode reflect"] + source_lines + ["entry " + entry, "reflection " + out]) + "\n")
    run_metal(binary, path)
    return hostenv.read_json(out)


def apple_gfx_reflection(binary, source_lines, pipeline, work):
    """{"vertex": {...}, "fragment": {...}}: each stage's buffers (with layouts), textures, samplers
    and unsupported bindings, from a render pipeline built with the `pipeline` manifest lines."""
    out = os.path.join(work, "apple-gfx-reflection.json")
    path = os.path.join(work, "apple-gfx-reflect.txt")
    hostenv.write_text(path, "\n".join(["mode reflect-gfx"] + source_lines + pipeline + ["reflection " + out]) + "\n")
    run_metal(binary, path)
    return hostenv.read_json(out)
