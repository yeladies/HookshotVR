"""Experimental Windows x64 OpenXR loader/extension probe; no game or XR session.

Run: python -I -B scripts/openxr_preflight.py [--loader ABSOLUTE_DLL_PATH]
Exit 0: enable2 advertised; 1: unavailable/unsupported; 2: invalid invocation.
An advertised extension is not proof of a connected HMD or usable Vulkan device.
"""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import sys


class ExtensionProperties(C.Structure):
    # XrExtensionProperties, Khronos OpenXR 1.0 ABI (openxr.h).
    _fields_ = [("type", C.c_int32), ("next", C.c_void_p),
                ("extensionName", C.c_char * 128), ("extensionVersion", C.c_uint32)]


def enumerate_extensions(enumerate_fn):
    """Retry the standard count/fill enumeration if the list grows."""
    for _ in range(3):
        count = C.c_uint32()
        result = enumerate_fn(None, 0, C.byref(count), None)
        if result != 0:
            raise RuntimeError(f"xrEnumerateInstanceExtensionProperties(count): {result}")
        if count.value > 4096:
            raise RuntimeError("Implausible extension count")
        if not count.value:
            return []
        capacity = count.value
        props = (ExtensionProperties * capacity)()
        for prop in props:
            prop.type = 2  # XR_TYPE_EXTENSION_PROPERTIES
        result = enumerate_fn(None, capacity, C.byref(count), props)
        if result == -11:  # XR_ERROR_SIZE_INSUFFICIENT
            continue
        if result != 0:
            raise RuntimeError(f"xrEnumerateInstanceExtensionProperties(fill): {result}")
        if count.value > capacity:
            raise RuntimeError("Runtime returned more extensions than capacity")
        return sorted({p.extensionName.decode("utf-8") for p in props[:count.value]})
    raise RuntimeError("Extension list changed repeatedly; retry with a stable runtime")


def probe(loader_path):
    # Restrict dependencies to the selected DLL's folder and Windows system folders.
    # Never search the current directory or PATH for a loader.
    loader = C.WinDLL(str(loader_path), winmode=0x100 | 0x800)
    fn = loader.xrEnumerateInstanceExtensionProperties
    fn.restype = C.c_int32
    fn.argtypes = [C.c_char_p, C.c_uint32, C.POINTER(C.c_uint32),
                   C.POINTER(ExtensionProperties)]
    extensions = enumerate_extensions(fn)
    return {"loader": str(loader_path), "extensions": extensions,
            "vulkan_enable2_advertised": "XR_KHR_vulkan_enable2" in extensions}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--loader", type=Path,
                        help="absolute path to a trusted x64 openxr_loader.dll")
    args = parser.parse_args(argv)
    if args.loader is not None and not args.loader.is_absolute():
        parser.error("--loader must be an absolute path")
    report = {"scope": "loader-extension-enumeration-only", "hmd_tested": False,
              "session_created": False, "vulkan_device_tested": False}
    if sys.platform != "win32" or C.sizeof(C.c_void_p) != 8:
        report["error"] = "This experimental probe requires Windows x64 Python"
    else:
        loader_path = args.loader or Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32/openxr_loader.dll"
        try:
            report.update(probe(loader_path))
        except (OSError, AttributeError, RuntimeError, UnicodeError) as error:
            report["error"] = str(error)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report.get("vulkan_enable2_advertised") else 1


if __name__ == "__main__":
    raise SystemExit(main())
