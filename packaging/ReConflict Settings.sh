#!/bin/bash
# Uses the same PortMaster environment and executable-local settings as the game.
launcher="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/ReConflict.sh"
exec bash "$launcher" --configure
