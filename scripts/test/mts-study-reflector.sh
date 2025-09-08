#!/bin/bash

native_listen_address="10.26.132.43:9091"
native_target_address="10.26.132.43:9001"
native_initiator_address="10.26.132.43:9002"

libfabric_listen_address="10.26.132.43:9090"
libfabric_target_address="10.26.132.43:8001"
libfabric_initiator_address="10.26.132.43:8002"
gpu_id="0"

script_dir="$(cd -- "$(dirname -- "$(readlink -f "${BASH_SOURCE[0]}")")" &>/dev/null && pwd)"
project_dir="$(realpath "${script_dir}/../..")"

"${project_dir}/build/native/fabrics-perf" --gpu "${gpu_id}" --target "${native_target_address}" --initiator "${native_initiator_address}" --listen "${native_listen_address}" &
PID_NATIVE=$!

"${project_dir}/build/libfabric/fabrics-perf" --gpu "${gpu_id}" --target "${libfabric_target_address}" --initiator "${libfabric_initiator_address}" --listen "${libfabric_listen_address}" &
PID_LIBFABRIC=$!

trap "kill $PID_NATIVE $PID_LIBFABRIC 2>/dev/null" SIGINT

wait $PID_NATIVE $PID_LIBFABRIC

exit 0
