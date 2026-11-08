"""Game-free failure/retry checks for the experimental OpenXR loader probe."""
import ctypes as C
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "openxr_preflight", Path(__file__).resolve().parents[1] / "scripts/openxr_preflight.py")
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class EnumerationTests(unittest.TestCase):
    def test_retry_and_initialize_output_headers(self):
        calls = []

        def fake(layer, capacity, count, props):
            self.assertIsNone(layer)
            calls.append(capacity)
            C.cast(count, C.POINTER(C.c_uint32))[0] = 1
            if not capacity:
                return 0
            self.assertEqual(props[0].type, 2)
            self.assertIsNone(props[0].next)
            if len(calls) == 2:
                return -11
            props[0].extensionName = b"XR_KHR_vulkan_enable2"
            return 0

        self.assertEqual(probe.enumerate_extensions(fake), ["XR_KHR_vulkan_enable2"])
        self.assertEqual(calls, [0, 1, 0, 1])

    def test_loader_runtime_error(self):
        with self.assertRaisesRegex(RuntimeError, "count.*-51"):
            probe.enumerate_extensions(lambda *args: -51)

    def test_empty_list(self):
        self.assertEqual(probe.enumerate_extensions(lambda *args: 0), [])

    def test_unstable_list_is_bounded(self):
        def fake(layer, capacity, count, props):
            C.cast(count, C.POINTER(C.c_uint32))[0] = 1
            return -11 if capacity else 0
        with self.assertRaisesRegex(RuntimeError, "changed repeatedly"):
            probe.enumerate_extensions(fake)

    def test_fill_error(self):
        def fake(layer, capacity, count, props):
            C.cast(count, C.POINTER(C.c_uint32))[0] = 1
            return -2 if capacity else 0
        with self.assertRaisesRegex(RuntimeError, "fill.*-2"):
            probe.enumerate_extensions(fake)

    def test_invalid_output_count(self):
        def fake(layer, capacity, count, props):
            C.cast(count, C.POINTER(C.c_uint32))[0] = 2 if capacity else 1
            return 0
        with self.assertRaisesRegex(RuntimeError, "more extensions"):
            probe.enumerate_extensions(fake)

    def test_implausible_allocation_rejected(self):
        def fake(layer, capacity, count, props):
            C.cast(count, C.POINTER(C.c_uint32))[0] = 4097
            return 0
        with self.assertRaisesRegex(RuntimeError, "Implausible"):
            probe.enumerate_extensions(fake)

    def test_relative_loader_rejected(self):
        with self.assertRaises(SystemExit) as error:
            probe.main(["--loader", "untrusted.dll"])
        self.assertEqual(error.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
