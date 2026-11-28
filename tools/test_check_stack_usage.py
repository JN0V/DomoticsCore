import os, subprocess, sys, tempfile, unittest

SCRIPT = os.path.join(os.path.dirname(__file__), "check_stack_usage.py")


def run(lines, *args):
    with tempfile.TemporaryDirectory() as d:
        with open(os.path.join(d, "Core.cpp.su"), "w") as f:
            f.write("".join(lines))
        return subprocess.run([sys.executable, SCRIPT, *args, d],
                              capture_output=True, text=True)


SMALL = "DomoticsCore-Core/src/Core.cpp:10:6:begin\t96\tstatic\n"
BIG = "DomoticsCore-Storage/src/S.cpp:20:6:logPromotedRecord\t1168\tstatic\n"


class T(unittest.TestCase):
    def test_oversized_frame_fails_and_names_function(self):
        r = run([SMALL, BIG], "--limit", "1000")
        self.assertEqual(r.returncode, 1)
        self.assertIn("logPromotedRecord", r.stdout)

    def test_within_limit_passes(self):
        self.assertEqual(run([SMALL, BIG], "--limit", "1200").returncode, 0)

    def test_off_path_frame_ignored(self):
        r = run(["/pkg/framework/x.c:1:1:app_entry_redefinable\t4160\tstatic\n", SMALL])
        self.assertEqual(r.returncode, 0)

    def test_cpp_scoped_name_kept_whole(self):
        r = run(["DomoticsCore-Core/src/Core.cpp:5:6:DomoticsCore::Core::begin()\t2000\tstatic\n"],
                "--limit", "1000")
        self.assertEqual(r.returncode, 1)
        self.assertIn("DomoticsCore::Core::begin()", r.stdout)

    def test_no_frames_fails(self):
        self.assertEqual(run([]).returncode, 1)


if __name__ == "__main__":
    unittest.main()
