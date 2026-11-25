# Linux CI runner

A self-hosted GitHub Actions runner in Docker that builds Chimera on Linux for
`.github/workflows/ci.yml` (label `chimera`). It only opens outgoing HTTPS connections to
GitHub, so the server needs no public address or open port.

## First start

1. On GitHub: Settings > Actions > Runners > New self-hosted runner, and copy the token
   from the `config.sh` line (it expires after one hour).
2. On the server, in this folder:

   ```sh
   RUNNER_TOKEN=<token> docker compose up -d --build
   docker compose logs -f
   ```

   The log ends with "Listening for Jobs" and the runner shows as Idle on GitHub.

The registration is kept in the `state` volume, so later `docker compose up -d` calls need
no token. To register again, remove the runner on GitHub and run
`docker compose down -v` (this also drops the ccache and the checkouts).

## Updating the tools

`docker compose build --no-cache && docker compose up -d` takes the newest Vulkan SDK and
runner. Pin versions with build arguments (`VULKAN_SDK_VERSION`, `RUNNER_VERSION`,
`CLANG_VERSION`) in `docker-compose.yml` under `build.args`.

## Safety

The repository is public. Workflows run here only on pushes to the repository itself,
never on pull requests from forks. Keep Settings > Actions > General > "Require approval
for all external contributors" on, because a pull request could add its own trigger. The
container can still reach your local network; put it on a host or firewall rule that
blocks it if that matters.

## Volumes

- `state`: the runner registration.
- `work`: checkouts with their submodules, so they are fetched, not cloned, each run.
- `ccache`: compiler cache (20 GB at most), which makes builds after the first one fast.
