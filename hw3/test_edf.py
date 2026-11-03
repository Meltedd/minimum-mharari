import json
import argparse
from pathlib import Path
from subprocess import run
from typing import Any


def parse_output(stdout: str) -> dict[str, Any]:
    """parses the expected edf output format"""
    lines = [line.strip() for line in stdout.splitlines()]
    output: dict[str, Any] = {}

    found_zero = False
    for line in lines:
        if line.startswith("0:"):
            found_zero = True
        if any([
            not line,
            not found_zero,
            ':' not in line,
        ]):
            continue

        l, r = line.split(':', 1)
        l = l.strip()
        r = r.strip()

        if l.isdigit():
            output.setdefault(l, []).append(r)
        else:
            output[l] = r

    return output


def compare_outputs(
    actual: dict[str, Any],
    expected: dict[str, Any],
) -> list[str]:
    """compare actual output to expected output"""

    errors = []

    for key, val in actual.items():
        if key not in expected:
            errors.append(f"unexpected key: {key}")
            continue

        if key.isdigit():
            for evt in val:
                if evt in expected[key]:
                    expected[key].remove(evt)
                else:
                    errors.append(f"unexpected output: '{key}: {evt}'")
            if not expected[key]:
                del expected[key]
        elif val.strip() == expected[key].strip():
            del expected[key]
        else:
            errors.append(
                f"mismatched '{key}':\n"
                f"  expect: {expected[key]}\n"
                f"  actual: {val}")

    for key, val in expected.items():
        if key.isdigit():
            for evt in val:
                errors.append(f"missing output: '{key}: {evt}'")
        else:
            errors.append(f"missing output: '{key}: {val}'")

    return errors


def test_output(
    inp: str,
    exp: dict[str, Any],
    exe: Path,
) -> list[str]:
    """run a test case, return list of errors"""
    res = run(
        [str(exe.absolute())],
        input=inp,
        capture_output=True,
        text=True,
    )

    if res.returncode:
        raise RuntimeError(
            f"Executable failed (code {res.returncode}).\n"
            f"STDERR:\n{res.stderr}\n"
            f"STDOUT:\n{res.stdout}\n"
        )

    return compare_outputs(parse_output(res.stdout), exp)

def test_memcheck(inp: str, exe: Path) -> list[str]:
    """run a test case in valgrind, return list of errors"""
    cmd = [
        "valgrind",
        "--leak-check=full",
        "--error-exitcode=93",
        f"{str(exe.absolute())}",
    ]
    res = run(cmd, input=inp, capture_output=True, text=True)

    if res.returncode == 93:
        return [f"memcheck failed:\n{res.stderr}"]
    elif res.returncode:
        raise RuntimeError(
            f"Executable failed (code {res.returncode}).\n"
            f"STDERR:\n{res.stderr}\n"
            f"STDOUT:\n{res.stdout}\n"
        )

    return []


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('edf_path', type=Path,
        help="path to the edf executable")
    parser.add_argument('tests_path', type=Path,
        help="path to the test.json file")

    args = parser.parse_args()

    assert args.edf_path.exists(), "bad edf path"
    assert args.tests_path.exists(), "bad tests path"

    with open(args.tests_path, 'r') as f:
        tests = json.load(f)

    failed = []
    leaks = []
    for i, test in enumerate(tests):
        print("=" * 50)
        print(f"test {i}: {repr(test['input'])}")
        print("testing output...", end="")
        errors = test_output(test['input'], test['output'], args.edf_path)
        if errors:
            print("failed.")
            for err in errors:
                print(err)
            failed.append(i)
        else:
            print("passed!")

        print("testing memleaks...", end="")
        errors = test_memcheck(test['input'], args.edf_path)
        if errors:
            print("failed.")
            for err in errors:
                print(err)
            leaks.append(i)
        else:
            print("passed!")

    print(f"failed tests: {repr(failed)}")
    print(f"leaked tests: {repr(failed)}")

