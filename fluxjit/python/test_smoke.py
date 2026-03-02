import ctypes as C
import math

from fluxjit import FluxJit, default_lib_path


def main():
    fx = FluxJit(default_lib_path())

    ir = {
        "stream": "Integrate",
        "dt": 0.5,
    }

    k = fx.compile(ir)

    n = 16
    in0 = (C.c_float * n)(*[float(i) for i in range(n)])
    in1 = (C.c_float * n)(*[2.0 for _ in range(n)])
    out0 = (C.c_float * n)()

    fx.run_f32(k, in0, in1, out0)

    for i in range(n):
        expected = float(i) + 2.0 * 0.5
        if not math.isclose(out0[i], expected, rel_tol=1e-6, abs_tol=1e-6):
            raise SystemExit(f"smoke test failed at i={i}: expected={expected}, got={out0[i]}")

    # Damage kernel: masked vs compacted strategy should produce equivalent results.
    health = (C.c_float * n)(*[-1.0, 0.0, 1.0, 2.5, 3.0, -0.5, 10.0, 0.25,
                              -2.0, 4.0, 5.0, -3.0, 6.0, 7.0, 8.0, -4.0])
    out_masked = (C.c_float * n)()
    out_compacted = (C.c_float * n)()
    alive_masked = (C.c_float * n)()
    alive_compacted = (C.c_float * n)()

    damage_ir = {
        "stream": "Damage",
        "damage": 1.0,
    }

    k_masked = fx.compile(damage_ir, strategy="masked")
    k_compacted = fx.compile(damage_ir, strategy="compacted")

    fx.run_f32(k_masked, health, None, out_masked, alive_masked)
    fx.run_f32(k_compacted, health, None, out_compacted, alive_compacted)

    for i in range(n):
        if not math.isclose(out_masked[i], out_compacted[i], rel_tol=1e-6, abs_tol=1e-6):
            raise SystemExit(
                f"damage output mismatch at i={i}: masked={out_masked[i]}, compacted={out_compacted[i]}"
            )
        if not math.isclose(alive_masked[i], alive_compacted[i], rel_tol=1e-6, abs_tol=1e-6):
            raise SystemExit(
                f"alive flag mismatch at i={i}: masked={alive_masked[i]}, compacted={alive_compacted[i]}"
            )

    fx.destroy_kernel(k_masked)
    fx.destroy_kernel(k_compacted)

    fx.destroy_kernel(k)
    fx.close()
    print("fluxjit smoke test: PASS")


if __name__ == "__main__":
    main()
