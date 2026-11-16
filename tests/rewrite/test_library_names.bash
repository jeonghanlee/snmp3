#!/usr/bin/env bash
# Library names cell: checks the installed names of the module's shared libraries in this tree, the real files,
# their SONAMEs, the unversioned links, and the names the consumers record. It builds and starts nothing.
# usage: test_library_names.bash --output NEW_DIRECTORY [--products DIRECTORY]
# exit status: 0 every check passed, 1 a check failed, 2 the tree could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd)"
readonly ROOT

function main {
    local output="" products="" arch
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) output="${2:-}"; shift 2 ;;
            --products) products="${2:-}"; shift 2 ;;
            *) printf "usage: test_library_names.bash --output NEW_DIRECTORY [--products DIRECTORY]\n" >&2; exit 2 ;;
        esac
    done
    [[ -n "${output}" ]] || { printf "test_library_names: --output is required\n" >&2; exit 2; }
    if [[ -z "${products}" ]]; then
        products="$(printf "%s\n" "${ROOT}"/bin/*/snmp3Ioc)"
        products="${products%/snmp3Ioc}"
    fi
    arch="${products##*/}"
    [[ -x "${products}/snmp3Ioc" ]] || { printf "test_library_names: products not found in %s\n" "${products}" >&2; exit 2; }
    command -v readelf > /dev/null || { printf "test_library_names: readelf not found\n" >&2; exit 2; }
    mkdir -m 700 "${output}" || { printf "test_library_names: cannot create %s\n" "${output}" >&2; exit 2; }
    perl "${HERE}/library_names.pl" "${ROOT}" "${arch}" "${output}"
    exit $?
}

main "$@"
