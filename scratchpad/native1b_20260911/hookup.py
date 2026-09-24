"""Lane NATIVE1b's cross-file hook-up, as a REFUSING script (ww-anchored-hookup).

Two existing files the lane does not own outright:

  * `NifSkope.pro`      -- the meshoptimizer cluster PARTITIONER the ladder
                           groups with (`meshopt_partitionClusters`) lives in
                           `lib/meshoptimizer/src/partition.cpp`, which the .pro
                           has never named. Adding a source means qmake before
                           make.
  * `src/nifcli.cpp`    -- the two exact ways back, `--native-no-ladder` and
                           `--native-no-occluders`: one declaration, one parse
                           pair, one argument pair on the `cmdLodgen` signature
                           and its call, the arming call, and the usage text.

Every anchor must match EXACTLY ONCE in the file's own bytes; `--check` is the
default and writes nothing; the CR count is asserted unchanged (both files are
LF-only, measured, not assumed).
"""
import sys

# ( file, mode, anchor, text )   mode: "after" | "replace"
EDITS = [
    ( "NifSkope.pro", "after",
      "\tlib/meshoptimizer/src/indexanalyzer.cpp \\",
      "\tlib/meshoptimizer/src/partition.cpp \\" ),

    ( "src/nifcli.cpp", "replace",
      "\tQString lgNativeDir, lgNativeVerifyLodo, lgNativeVerifyLodi, lgNativeFixture, lgNativeMeshReport;\n\tbool lgNativeVerifyCorpus = false;",
      "\tQString lgNativeDir, lgNativeVerifyLodo, lgNativeVerifyLodi, lgNativeFixture, lgNativeMeshReport;\n"
      "\tbool lgNativeVerifyCorpus = false;\n"
      "\t// v3 (lane NATIVE1b): the two exact ways back off the ladder and the occluders\n"
      "\tbool lgNativeLadder = true, lgNativeOccluders = true;" ),

    ( "src/nifcli.cpp", "after",
      "\t\telse if ( t == QLatin1String( \"--native-verify-corpus\" ) ) lgNativeVerifyCorpus = true;",
      "\t\telse if ( t == QLatin1String( \"--native-no-ladder\" ) ) lgNativeLadder = false;\n"
      "\t\telse if ( t == QLatin1String( \"--native-no-occluders\" ) ) lgNativeOccluders = false;" ),

    ( "src/nifcli.cpp", "replace",
      "\tconst QString & nativeVerifyLodi, const QString & nativeFixture, const QString & nativeMeshReport,\n\tbool nativeVerifyCorpus )",
      "\tconst QString & nativeVerifyLodi, const QString & nativeFixture, const QString & nativeMeshReport,\n"
      "\tbool nativeVerifyCorpus, bool nativeLadder, bool nativeOccluders )" ),

    ( "src/nifcli.cpp", "replace",
      "\t\t\tlgNativeMeshReport, lgNativeVerifyCorpus );",
      "\t\t\tlgNativeMeshReport, lgNativeVerifyCorpus, lgNativeLadder, lgNativeOccluders );" ),

    ( "src/nifcli.cpp", "replace",
      "\t\t\tlodgenNativeBegin( &world, nativeDir, lodgenNativeLoadModel, const_cast<QString *>( &nativeDataRoot ),\n\t\t\t\tnativeMeshReport );",
      "\t\t\tlodgenNativeBegin( &world, nativeDir, lodgenNativeLoadModel, const_cast<QString *>( &nativeDataRoot ),\n"
      "\t\t\t\tnativeMeshReport, nativeLadder, nativeOccluders );" ),

    ( "src/nifcli.cpp", "after",
      "\t  << \"  lodgen --native-fixture <dir>           write the synthetic known-answer pair\\n\"",
      "\t  << \"  lodgen ... --native <dir> --native-no-ladder\\n\"\n"
      "\t  << \"                                          one level a mesh (every error 0),\\n\"\n"
      "\t  << \"                                          the exact way back off the v3\\n\"\n"
      "\t  << \"                                          cluster ladder\\n\"\n"
      "\t  << \"  lodgen ... --native <dir> --native-no-occluders\\n\"\n"
      "\t  << \"                                          write no occluder boxes; the\\n\"\n"
      "\t  << \"                                          census says so in words\\n\"" ),
]


def main():
    apply = "--apply" in sys.argv
    files = {}
    for path, _mode, _anchor, _text in EDITS:
        if path not in files:
            raw = open(path, "rb").read()
            files[path] = {"raw": raw, "cr": raw.count(b"\r"), "text": raw.decode("utf-8")}
    ok = True
    for path, mode, anchor, text in EDITS:
        body = files[path]["text"]
        n = body.count(anchor)
        label = anchor.split("\n")[0][:64]
        print("%-16s %-8s %-66s x%d" % (path, mode, label, n))
        if n != 1:
            ok = False
    if not ok:
        print("REFUSED: every anchor must match exactly once")
        return 1
    for path, mode, anchor, text in EDITS:
        body = files[path]["text"]
        if mode == "after":
            files[path]["text"] = body.replace(anchor, anchor + "\n" + text, 1)
        else:
            files[path]["text"] = body.replace(anchor, text, 1)
    for path, d in files.items():
        data = d["text"].encode("utf-8")
        print("%-16s %d -> %d bytes, CR %d -> %d"
              % (path, len(d["raw"]), len(data), d["cr"], data.count(b"\r")))
        if data.count(b"\r") != d["cr"]:
            print("REFUSED: CR count moved in %s" % path)
            return 1
    if not apply:
        print("--check only: nothing written")
        return 0
    for path, d in files.items():
        open(path, "wb").write(d["text"].encode("utf-8"))
    print("applied")
    return 0


sys.exit(main())
