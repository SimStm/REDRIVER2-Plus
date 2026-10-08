"""Run trusted local capture-analysis scripts without the rdc daemon/RPC timeout.

Use the Python environment containing rdc-cli and its configured RenderDoc1.41.
Capture/export outputs may contain private game assets: keep them in ignored
build directories. The caller monitors this process; observation timeouts do not
terminate or restart it. Each script receives rd, controller and a fresh args dict.
"""

import argparse
import json
import time
from pathlib import Path


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--script", type=Path, action="append", required=True)
    parser.add_argument("--arg", action="append", default=[], metavar="KEY=VALUE")
    parser.add_argument("--gpu", required=True, help="Unique replay GPU name substring")
    parser.add_argument("--summary", type=Path, required=True)
    options = parser.parse_args()
    arguments = {}
    for value in options.arg:
        key, separator, content = value.partition("=")
        if not separator or not key:
            parser.error("--arg requires a nonempty KEY=VALUE")
        if key in arguments:
            parser.error("duplicate --arg key: " + key)
        arguments[key] = content
    for path in [options.capture] + options.script:
        if not path.is_file():
            parser.error("file not found: " + str(path))
    return options, arguments


def run(options, arguments):
    from rdc.discover import find_renderdoc

    rd = find_renderdoc()
    if rd is None or rd.GetVersionString() != "1.41":
        raise RuntimeError("Configured RenderDoc1.41 Python module is required")
    capture = controller = None
    initialized = False
    started = time.monotonic()
    report = {"capture": str(options.capture.resolve()), "renderdoc": "1.41", "scripts": []}
    try:
        rd.InitialiseReplay(rd.GlobalEnvironment(), [])
        initialized = True
        capture = rd.OpenCaptureFile()
        status = capture.OpenFile(str(options.capture.resolve()), "", None)
        if status != rd.ResultCode.Succeeded:
            raise RuntimeError("OpenFile failed: " + str(status))
        if capture.LocalReplaySupport() == rd.ReplaySupport.Unsupported:
            raise RuntimeError("This capture does not support local replay")
        matches = [gpu for gpu in capture.GetAvailableGPUs()
                   if options.gpu.casefold() in gpu.name.casefold()]
        if len(matches) != 1:
            raise RuntimeError("Replay GPU must match exactly once; available: " +
                               ", ".join(gpu.name for gpu in capture.GetAvailableGPUs()))
        gpu = matches[0]
        replay = rd.ReplayOptions()
        replay.forceGPUVendor = gpu.vendor
        replay.forceGPUDeviceID = gpu.deviceID
        report["gpu"] = {"name": gpu.name, "deviceID": gpu.deviceID}
        status, controller = capture.OpenCapture(replay, None)
        if status != rd.ResultCode.Succeeded or controller is None:
            raise RuntimeError("OpenCapture failed: " + str(status))
        for script in options.script:
            print("Starting analysis: " + str(script), flush=True)
            phase_started = time.monotonic()
            scope = {"rd": rd, "controller": controller, "args": dict(arguments),
                     "__file__": str(script.resolve()), "__name__": "__renderdoc_analysis__"}
            exec(compile(script.read_text(encoding="utf-8-sig"), str(script), "exec"), scope)
            elapsed = time.monotonic() - phase_started
            report["scripts"].append({"path": str(script.resolve()), "seconds": elapsed,
                                      "result": scope.get("result")})
            print("Finished analysis in %.3fs: %s" % (elapsed, script), flush=True)
        report["seconds"] = time.monotonic() - started
        options.summary.parent.mkdir(parents=True, exist_ok=True)
        options.summary.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print("Analysis complete; summary: " + str(options.summary), flush=True)
    finally:
        # Controller references the capture; close in ownership order on failure too.
        try:
            if controller is not None:
                controller.Shutdown()
        finally:
            try:
                if capture is not None:
                    capture.Shutdown()
            finally:
                if initialized:
                    rd.ShutdownReplay()


if __name__ == "__main__":
    run(*parse_arguments())
