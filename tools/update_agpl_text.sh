#!/usr/bin/env bash
# Replaces the AGPL-3.0 text at the end of LICENSE with the official text from gnu.org.
# The project header at the top of the file (Meta AI credit, third-party notices) is kept.
set -euo pipefail
cd "$(dirname "$0")/.."
marker='==== GNU AFFERO GENERAL PUBLIC LICENSE v3 - FULL TEXT BELOW ===='
curl -fsSL https://www.gnu.org/licenses/agpl-3.0.txt -o /tmp/agpl-3.0.txt
awk -v m="$marker" '{print} index($0,m){exit}' LICENSE > /tmp/LICENSE.head
{
  cat /tmp/LICENSE.head
  echo "======================================================================"
  echo
  cat /tmp/agpl-3.0.txt
} > LICENSE
echo "LICENSE updated with the official AGPL-3.0 text."
