#! /bin/bash

script_dir="$(cd -- "$(dirname -- "$(readlink -f "${BASH_SOURCE[0]}")")" &> /dev/null && pwd)"
project_dir="$(realpath "${script_dir}/../..")"
fabrics_perf="${project_dir}/build/Linux-Clang-Release/fabrics-perf"

formats=(720 1080 2160)
tests=(Cuda2Cuda Host2Host)
methods=(Wait Spin)

for method in "${methods[@]}"; do
    for format in "${formats[@]}"; do
        for test in "${tests[@]}"; do
            echo "Running test ${test} with image format ${format}"
            "${fabrics_perf}" --run "MXLFabrics+${test}+Verbs+Reflect+${method}" \
                --output "output/h2h-vs-c2c-data/test-${format}" \
                -f "${project_dir}/config/flow-${format}.json" "${@}"
        done
    done
done
