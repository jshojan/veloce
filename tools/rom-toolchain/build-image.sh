#!/usr/bin/env bash
# tools/rom-toolchain/build-image.sh — SH-3a
#
# Builds the rom-toolchain image (docker build), then finishes the "image
# label with tool versions" requirement in two steps that a single `docker
# build` can't do on its own:
#   1. `docker build` — every tool stage already wrote its own real
#      `<tool> --version` output to /opt/rom-toolchain/versions/<tool>.txt
#      inside the image (see the Dockerfile).
#   2. This script reads those files back out with `docker run`, then
#      `docker commit --change 'LABEL tool.<name>.version=...'` (one change
#      per tool) onto the built image, and writes the same map to
#      tools/rom-toolchain/versions.json for tests/roms-src/build.py to fold
#      into rom_manifest.json.
#
# Usage: tools/rom-toolchain/build-image.sh [tag]
#   default tag veloce/rom-toolchain:dev, the image tests/roms-src/build.py uses
#   by default (override there with VELOCE_ROM_TOOLCHAIN_IMAGE or --image).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TAG="${1:-veloce/rom-toolchain:dev}"
BUILD_TAG="${TAG}-prelabel"

echo "== docker build -t ${BUILD_TAG} ==" >&2
# The Dockerfile COPYs nothing from the context (every tool is fetched at a
# pinned SHA), so send an empty context rather than the whole repo (build
# dirs, ROM checkouts): faster, and nothing local can leak into the image.
EMPTY_CTX="$(mktemp -d)"
trap 'rm -rf "${EMPTY_CTX}"' EXIT
docker build -f "${HERE}/Dockerfile" -t "${BUILD_TAG}" "${EMPTY_CTX}"

echo "== collecting tool versions ==" >&2
VERSIONS_JSON="${HERE}/versions.json"
TMP_LIST="$(mktemp)"
docker run --rm "${BUILD_TAG}" \
  sh -c 'for f in /opt/rom-toolchain/versions/*.txt; do n=$(basename "$f" .txt); printf "%s\t" "$n"; cat "$f" | tr "\n" " "; printf "\n"; done' \
  > "${TMP_LIST}"

python3 - "${TMP_LIST}" "${VERSIONS_JSON}" "${BUILD_TAG}" <<'PYEOF'
import json, sys, subprocess

tmp_list, out_path, build_tag = sys.argv[1:4]
versions = {}
with open(tmp_list) as f:
    for line in f:
        line = line.rstrip("\n")
        if not line.strip():
            continue
        name, _, value = line.partition("\t")
        versions[name] = value.strip()

digest = subprocess.run(
    ["docker", "inspect", "--format", "{{index .RepoDigests 0}}", build_tag],
    capture_output=True, text=True,
).stdout.strip()

json.dump({"tools": versions, "image_predigest": digest or None}, open(out_path, "w"), indent=2, sort_keys=True)
json.dump  # noqa
print(f"wrote {out_path} with {len(versions)} tool versions", file=sys.stderr)
PYEOF

CHANGE_ARGS=()
while IFS=$'\t' read -r name value; do
  [ -z "$name" ] && continue
  # LABEL values can't contain newlines (already tr'd out); escape backslash
  # and double-quote for Dockerfile-instruction-string syntax.
  safe_value=$(printf '%s' "$value" | sed 's/\\/\\\\/g; s/"/\\"/g')
  CHANGE_ARGS+=(--change "LABEL tool.${name}.version=\"${safe_value}\"")
done < "${TMP_LIST}"
rm -f "${TMP_LIST}"

echo "== docker commit with ${#CHANGE_ARGS[@]} version labels ==" >&2
CONTAINER_ID=$(docker create "${BUILD_TAG}")
# shellcheck disable=SC2086
docker commit "${CHANGE_ARGS[@]}" \
  --change "LABEL org.opencontainers.image.title=veloce-rom-toolchain" \
  "${CONTAINER_ID}" "${TAG}"
docker rm "${CONTAINER_ID}" >/dev/null
docker rmi "${BUILD_TAG}" >/dev/null 2>&1 || true

echo "== labeled image: ${TAG} ==" >&2
docker inspect --format '{{json .Config.Labels}}' "${TAG}" | python3 -m json.tool 2>/dev/null \
  || docker inspect --format '{{json .Config.Labels}}' "${TAG}"
