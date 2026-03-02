import ctypes as C
import json
from pathlib import Path


class FluxJit:
    def __init__(self, lib_path: str):
        self._lib = C.CDLL(lib_path)

        self._lib.flux_create.restype = C.c_void_p
        self._lib.flux_destroy.argtypes = [C.c_void_p]

        self._lib.flux_compile_json.argtypes = [C.c_void_p, C.c_char_p, C.POINTER(C.c_char_p)]
        self._lib.flux_compile_json.restype = C.c_void_p
        self._lib.flux_kernel_destroy.argtypes = [C.c_void_p]

        self._lib.flux_run_f32.argtypes = [
            C.c_void_p,
            C.c_int64,
            C.POINTER(C.c_float),
            C.POINTER(C.c_float),
            C.POINTER(C.c_float),
            C.POINTER(C.c_float),
            C.POINTER(C.c_char_p),
        ]
        self._lib.flux_run_f32.restype = C.c_int

        self._lib.flux_free_string.argtypes = [C.c_char_p]

        self._ctx = self._lib.flux_create()
        if not self._ctx:
            raise RuntimeError("flux_create failed")

    def close(self):
        if self._ctx:
            self._lib.flux_destroy(self._ctx)
            self._ctx = None

    def __del__(self):
        self.close()

    def compile(self, ir: dict, strategy: str | None = None):
        payload_dict = dict(ir)
        if strategy is not None:
            payload_dict["strategy"] = strategy

        err = C.c_char_p()
        payload = json.dumps(payload_dict).encode("utf-8")
        kernel = self._lib.flux_compile_json(self._ctx, payload, C.byref(err))
        if not kernel:
            msg = err.value.decode("utf-8") if err.value else "unknown compile error"
            if err.value:
                self._lib.flux_free_string(err)
            raise RuntimeError(msg)
        return kernel

    def run_f32(self, kernel, in0, in1, out0, out1=None):
        def _is_c_float_array(x):
            return hasattr(x, "_type_") and x._type_ is C.c_float

        def _to_ptr_and_len(x, name):
            if _is_c_float_array(x):
                return x, len(x)

            if hasattr(x, "ctypes") and hasattr(x, "shape") and hasattr(x, "dtype"):
                # Optional NumPy-style support without importing NumPy.
                return x.ctypes.data_as(C.POINTER(C.c_float)), int(x.shape[0])

            raise TypeError(f"{name} must be ctypes c_float array (or NumPy float32 array)")

        in0_ptr, n0 = _to_ptr_and_len(in0, "in0")
        out0_ptr, n2 = _to_ptr_and_len(out0, "out0")

        if in1 is not None:
            in1_ptr, n1 = _to_ptr_and_len(in1, "in1")
            if not (n0 == n1 == n2):
                raise ValueError("array lengths must match")
        else:
            in1_ptr = C.POINTER(C.c_float)()
            if n0 != n2:
                raise ValueError("in0 and out0 lengths must match")

        n = n0

        err = C.c_char_p()
        if out1 is not None:
            out1_ptr, n3 = _to_ptr_and_len(out1, "out1")
            if n3 != n:
                raise ValueError("out1 length must match input length")
        else:
            out1_ptr = C.POINTER(C.c_float)()

        rc = self._lib.flux_run_f32(
            kernel,
            C.c_int64(n),
            in0_ptr,
            in1_ptr,
            out0_ptr,
            out1_ptr,
            C.byref(err),
        )
        if rc != 0:
            msg = err.value.decode("utf-8") if err.value else f"runtime error code {rc}"
            if err.value:
                self._lib.flux_free_string(err)
            raise RuntimeError(msg)

    def destroy_kernel(self, kernel):
        self._lib.flux_kernel_destroy(kernel)


def default_lib_path() -> str:
    here = Path(__file__).resolve()
    return str((here.parent.parent / "build" / "libfluxjit.so").resolve())
