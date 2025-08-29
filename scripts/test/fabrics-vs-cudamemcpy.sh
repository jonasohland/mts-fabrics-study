#! /bin/bash

script_dir="$(cd -- "$(dirname -- "$(readlink -f "${BASH_SOURCE[0]}")")" &>/dev/null && pwd)"
project_dir="$(realpath "${script_dir}/../..")"
fabrics_perf="${project_dir}/build/Linux-Clang-Release/fabrics-perf"

formats=(720 1080 2160)
tests=(Cuda2Host Host2Cuda)

function run_test() {
    case="${1}"
    format="${2}"
    shift
    shift

    echo -------- testing: "${case}" "${format}"

    "${fabrics_perf}" --run "${case}" \
        --output "output/fabrics-vs-cudamemcpy/test-${format}" \
        -f "${project_dir}/config/flow-${format}.json" \
        --initiator runner-tx --target runner-rx \
        "${@}"
}

for format in "${formats[@]}"; do
    for test in "${tests[@]}"; do
        run_test "NativeCuda+${test}" "${format}" "${@}"
        run_test "MXLFabrics+${test}+SHM+OneWay+Wait" "${format}" "${@}"
        run_test "MXLFabrics+${test}+SHM+OneWay+Spin" "${format}" "${@}"
    done
done
