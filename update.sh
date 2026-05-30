#!/bin/bash
# YataiDON Linux Updater
#
# Expected GitHub release assets:
#   checksums-linux.sha256  sha256sum-format, relative paths from install dir
#                           (excludes git-managed skins)
#   skin-manifest.tsv       path<TAB>repo_url<TAB>commit<TAB>checksums_url
#   update-linux.tar.gz     binary + shader + lib
#
# Skin repos must contain a checksums.sha256 at their root.
#
# Usage (standalone):   ./update.sh
# Usage (from game):    ./update.sh --wait-pid <PID>
#
# Requires: curl, python3, sha256sum, tar

set -euo pipefail

REPO="yonokid/YataiDON"
API_URL="https://api.github.com/repos/$REPO/releases/latest"
INSTALL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION_FILE="$INSTALL_DIR/.version"
TMP_DIR="$(mktemp -d)"
WAIT_PID=""

cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT

while [[ $# -gt 0 ]]; do
    case "$1" in
        --wait-pid) WAIT_PID="$2"; shift 2 ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

die() { echo "[update] Error: $*" >&2; exit 1; }
log() { echo "[update] $*"; }

# Derive raw file URL from repo URL + commit + filepath
skin_raw_url() {
    local repo_url="$1" commit="$2" filepath="$3"
    local base="${repo_url%.git}"
    if [[ "$base" == *"github.com"* ]]; then
        echo "${base/github.com/raw.githubusercontent.com}/$commit/$filepath"
    else
        echo "$base/raw/commit/$commit/$filepath"
    fi
}

# --- Fetch release metadata ---
log "Checking for updates..."
RELEASE_JSON=$(curl -sfL --max-time 10 "$API_URL") || die "GitHub API unreachable"

LATEST_TAG=$(python3 -c "import json,sys; print(json.loads(sys.stdin.read())['tag_name'])" <<< "$RELEASE_JSON")
[ -z "$LATEST_TAG" ] && die "Could not parse tag_name from API response"

LOCAL_TAG=$(cat "$VERSION_FILE" 2>/dev/null || echo "none")
log "Local: $LOCAL_TAG | Latest: $LATEST_TAG"

python3 -c "
import json, sys
for a in json.loads(sys.stdin.read()).get('assets', []):
    print(a['name'] + '\t' + a['browser_download_url'])
" <<< "$RELEASE_JSON" > "$TMP_DIR/assets.tsv"

asset_url() {
    awk -F'\t' -v name="$1" '$1==name {print $2}' "$TMP_DIR/assets.tsv"
}

# --- Download checksums and skin manifest ---
CHECKSUMS_URL=$(asset_url "checksums-linux.sha256")
[ -z "$CHECKSUMS_URL" ] && die "No checksums-linux.sha256 in release $LATEST_TAG"
curl -sfL --max-time 30 -o "$TMP_DIR/checksums.sha256" "$CHECKSUMS_URL"

SKIN_MANIFEST_URL=$(asset_url "skin-manifest.tsv")
if [ -n "$SKIN_MANIFEST_URL" ]; then
    curl -sfL --max-time 10 -o "$TMP_DIR/skin-manifest.tsv" "$SKIN_MANIFEST_URL"
else
    touch "$TMP_DIR/skin-manifest.tsv"
fi

# --- Check main package ---
NEED_PACKAGE=0
while read -r expected_hash rel_path; do
    local_file="$INSTALL_DIR/$rel_path"
    if [ -f "$local_file" ]; then
        actual_hash=$(sha256sum "$local_file" | awk '{print $1}')
        [ "$actual_hash" = "$expected_hash" ] && continue
    fi
    NEED_PACKAGE=1
    break
done < "$TMP_DIR/checksums.sha256"

# --- Check installed skins against manifest ---
NEED_SKIN_COUNT=0
while IFS=$'\t' read -r skin_path repo_url expected_commit checksums_url; do
    [ -z "$skin_path" ] && continue
    local_version_file="$INSTALL_DIR/$skin_path/.skin-version"
    local_commit="none"
    [ -f "$local_version_file" ] && local_commit=$(cat "$local_version_file")
    [ "$local_commit" != "$expected_commit" ] && NEED_SKIN_COUNT=$((NEED_SKIN_COUNT+1))
done < "$TMP_DIR/skin-manifest.tsv"

if [ $NEED_PACKAGE -eq 0 ] && [ $NEED_SKIN_COUNT -eq 0 ]; then
    log "Already up to date."
    echo "$LATEST_TAG" > "$VERSION_FILE"
    exit 0
fi

log "Updates needed — package: $NEED_PACKAGE | skins: $NEED_SKIN_COUNT"

# --- Wait for game process if requested ---
if [ -n "$WAIT_PID" ]; then
    log "Waiting for game (PID $WAIT_PID) to exit..."
    while kill -0 "$WAIT_PID" 2>/dev/null; do sleep 0.25; done
fi

# --- Download and extract main package ---
if [ $NEED_PACKAGE -eq 1 ]; then
    url=$(asset_url "update-linux.tar.gz")
    [ -z "$url" ] && die "No update-linux.tar.gz in release $LATEST_TAG"
    log "Downloading update-linux.tar.gz..."
    curl -fL --progress-bar -o "$TMP_DIR/update-linux.tar.gz" "$url"
    log "Extracting..."
    tar -xzf "$TMP_DIR/update-linux.tar.gz" -C "$INSTALL_DIR"
    chmod +x "$INSTALL_DIR/YataiDON"
    log "Package applied."
fi

# --- Update skins ---
if [ $NEED_SKIN_COUNT -gt 0 ]; then
    while IFS=$'\t' read -r skin_path repo_url expected_commit checksums_url; do
        [ -z "$skin_path" ] && continue
        local_dir="$INSTALL_DIR/$skin_path"
        local_version_file="$local_dir/.skin-version"
        local_commit="none"
        [ -f "$local_version_file" ] && local_commit=$(cat "$local_version_file")
        [ "$local_commit" = "$expected_commit" ] && continue

        log "Checking $skin_path..."
        curl -sfL --max-time 30 -o "$TMP_DIR/skin-checksums.sha256" "$checksums_url" || {
            log "Warning: could not fetch checksums for $skin_path — skipping"
            continue
        }

        changed=0
        while read -r expected_hash rel_path; do
            local_file="$local_dir/$rel_path"
            if [ -f "$local_file" ]; then
                actual_hash=$(sha256sum "$local_file" | awk '{print $1}')
                [ "$actual_hash" = "$expected_hash" ] && continue
            fi
            raw_url=$(skin_raw_url "$repo_url" "$expected_commit" "$rel_path")
            mkdir -p "$(dirname "$local_file")"
            curl -sfL -o "$local_file.tmp" "$raw_url" && mv "$local_file.tmp" "$local_file" || {
                log "Warning: failed to download $rel_path"
                rm -f "$local_file.tmp"
                continue
            }
            changed=$((changed+1))
        done < "$TMP_DIR/skin-checksums.sha256"

        echo "$expected_commit" > "$local_version_file"
        log "$skin_path: $changed file(s) updated."
    done < "$TMP_DIR/skin-manifest.tsv"
fi

echo "$LATEST_TAG" > "$VERSION_FILE"
log "Update complete ($LATEST_TAG). Restart YataiDON to apply."
