#!/usr/bin/env bash
# Registers the runner the first time (needs RUNNER_TOKEN) and keeps its registration in
# the state volume, so recreating the container does not need a new token.
set -euo pipefail

cd /home/runner/actions-runner
state=/home/runner/state
files=(.runner .credentials .credentials_rsaparams)

if [ -f "$state/.runner" ]; then
  for file in "${files[@]}"; do
    cp "$state/$file" .
  done
else
  if [ -z "${RUNNER_TOKEN:-}" ]; then
    echo "RUNNER_TOKEN is empty and the runner was never registered." >&2
    echo "Get a token from the repository: Settings > Actions > Runners > New self-hosted runner." >&2
    exit 1
  fi
  ./config.sh --unattended \
    --url "${REPO_URL}" \
    --token "${RUNNER_TOKEN}" \
    --name "${RUNNER_NAME:-chimera-linux}" \
    --labels chimera \
    --work /home/runner/work \
    --replace
  for file in "${files[@]}"; do
    cp "$file" "$state/"
  done
fi

exec ./run.sh
