#! /bin/bash

script_dir="$(cd -- "$(dirname -- "$(readlink -f "${BASH_SOURCE[0]}")")" &>/dev/null && pwd)"
project_dir="$(realpath "${script_dir}/../..")"
output_root="${project_dir}/output/mts_study"

declare -A reflector_listener=(["libfabric"]="10.26.132.43:9090" ["native"]="10.26.132.43:9091")
runner_initiator_address="10.26.132.23:8001"
runner_target_address="10.26.132.23:8002"
gpu_id=0

nb_iter="200"
formats=(720 1080 2160)
libraries=("native" "libfabric")

# function get_args(target, initiator, output_dir, gpu_ids, test_name, connect, flow, nb_iter) {
#
# }
#
# Device to Device Inter-Host
function run_d2d_interhost() {
  methods=(Wait Spin)
  tests=(Cuda2Cuda)
  for library in "${libraries[@]}"; do
    for method in "${methods[@]}"; do
      for format in "${formats[@]}"; do
        for test in "${tests[@]}"; do
          test_name="MXLFabrics+${test}+Verbs+Reflect+${method}"
          format_file="${project_dir}/config/flow-${format}.json"
          output_dir="${output_root}/d2d-interhost/${format}/${library}"
          args="--target ${runner_target_address} \
          --initiator ${runner_initiator_address} \
          --output ${output_dir} \
          --gpu ${gpu_id} \
          --run ${test_name} \
          --connect ${reflector_listener[$library]} \
          --flow ${format_file} \
          --iterations ${nb_iter}"

          echo "Running test \"${test_name}\" with image format \"${format}\" and output directory \"${output_dir}\" peer \"${reflector_listener[$library]}\""

          "${project_dir}/build/${library}/fabrics-perf" ${args}
        done
      done
    done
  done
}

# Device to Host 2 Host to Device Inter-Host
function run_dh2hd_interhost() {
  methods=(Wait Spin)
  tests=(Cuda2Cuda Cuda2Host2Host2Cuda)
  for library in "${libraries[@]}"; do
    for method in "${methods[@]}"; do
      for format in "${formats[@]}"; do
        for test in "${tests[@]}"; do
          test_name="MXLFabrics+${test}+Verbs+Reflect+${method}"
          format_file="${project_dir}/config/flow-${format}.json"
          output_dir="${output_root}/dh2hd-interhost/${format}/${library}"
          args="--target ${runner_target_address} \
          --initiator ${runner_initiator_address} \
          --output ${output_dir} \
          --gpu ${gpu_id} \
          --run ${test_name} \
          --connect ${reflector_listener[$library]} \
          --flow ${format_file} \
          --iterations ${nb_iter}"

          echo "Running test \"${test_name}\" with image format \"${format}\" and output directory \"${output_dir}\" peer \"${reflector_listener[$library]}\""

          "${project_dir}/build/${library}/fabrics-perf" ${args}
        done
      done
    done
  done
}

# Host to Device Intra-Host

# Device to Host Intra-Host

# Device to Device Intra-Host Inter-GPU

# run_d2d_interhost
run_dh2hd_interhost
