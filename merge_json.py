#!/usr/bin/env python3
import json, glob, os, sys

# Allow optional directory argument
target_dir = sys.argv[1] if len(sys.argv) > 1 else "."

output_file = os.path.join(target_dir, "combined_colintrace.json")
json_files = sorted(glob.glob(os.path.join(target_dir, "*.json")))

if not json_files:
    print(f"[merge_traces] No JSON files found in {target_dir}")
    exit(1)

combined_events = []

for fname in json_files:
    if os.path.basename(fname) == "combined_colintrace.json":
        continue
    with open(fname, "r") as f:
        try:
            data = json.load(f)
            if isinstance(data, dict) and "traceEvents" in data:
                combined_events.extend(data["traceEvents"])
            elif isinstance(data, list):
                combined_events.extend(data)
        except json.JSONDecodeError:
            print(f"[merge_traces] Skipping invalid JSON: {fname}")

with open(output_file, "w") as out:
    json.dump({"traceEvents": combined_events}, out, indent=2)

print(f"[merge_traces] Combined {len(json_files)} files into {output_file}")
