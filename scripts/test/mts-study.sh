#! /bin/bash

PURPLE="\033[0;35m"
CYAN="\033[0;36m"
NC="\033[0m"

script_dir="$(cd -- "$(dirname -- "$(readlink -f "${BASH_SOURCE[0]}")")" &>/dev/null && pwd)"
project_dir="$(realpath "${script_dir}/../..")"
output_root="${project_dir}/output/mts_study"

declare -A remote_reflector_listener=(["libfabric"]="10.26.132.43:9090" ["native"]="10.26.132.43:9091")
declare -A local_reflector_listener=(["libfabric"]="127.0.0.1:9090" ["native"]="127.0.0.1:9091")

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
  echo -e "${CYAN}Starting Device-to-Device Inter-Host transfers${NC}"

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
          --connect ${remote_reflector_listener[$library]} \
          --flow ${format_file} \
          --iterations ${nb_iter}"

          echo -e "${PURPLE}Running test \"${test_name}\" with image format \"${format}\" and output directory \"${output_dir}\" peer \"${remote_reflector_listener[$library]}\"${NC}"

          "${project_dir}/build/${library}/fabrics-perf" ${args}
        done
      done
    done
  done
}

# Device to Host 2 Host to Device Inter-Host
function run_dh2hd_interhost() {
  echo -e "\033[0;36mStarting Host-to-Device-to-Host-to-Device Inter-Host transfers\033[0m"

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
          --connect ${remote_reflector_listener[$library]} \
          --flow ${format_file} \
          --iterations ${nb_iter}"

          echo -e "${PURPLE}Running test \"${test_name}\" with image format \"${format}\" and output directory \"${output_dir}\" peer \"${remote_reflector_listener[$library]}\"${NC}"

          "${project_dir}/build/${library}/fabrics-perf" ${args}
        done
      done
    done
  done
}

# Host to Device Intra-Host
function run_h2d_intrahost() {
  echo -e "${CYAN}Starting Host-to-Device Intra-Host transfers${NC}"
  methods=(Wait Spin)

  for format in "${formats[@]}"; do
    format_file="${project_dir}/config/flow-${format}.json"

    # libfabric
    for method in "${methods[@]}"; do
      test_name="MXLFabrics+Host2Cuda+SHM+OneWay+${method}"
      output_dir="${output_root}/h2d-intrahost/${format}/libfabric"
      args="--target ${runner_target_address} \
        --initiator ${runner_initiator_address} \
        --output ${output_dir} \
        --gpu ${gpu_id} \
        --run ${test_name} \
        --connect ${local_reflector_listener["libfabric"]} \
        --flow ${format_file} \
        --iterations ${nb_iter}"

      echo -e "${PURPLE}Running test \"${test_name}\" with image format \"${format}\" and output directory \"${output_dir}\" peer \"${local_reflector_listener["libfabric"]}\"${NC}"

      "${project_dir}/build/libfabric/fabrics-perf" ${args}
    done

    # NativeCuda
    test_name="NativeCuda+Host2Cuda"
    output_dir="${output_root}/h2d-intrahost/${format}/native"

    # addresses are dummy, no peer is required
    args="--target 127.0.0.1:10000 \
        --initiator 127.0.0.1:10001 \
        --output ${output_dir} \
        --gpu ${gpu_id} \
        --run ${test_name} \
        --connect 127.0.0.1:8080 \
        --flow ${format_file} \
        --iterations ${nb_iter}"

    echo -e "${PURPLE}Running test \"${test_name}\" with image format \"${format}\" and output directory \"${output_dir}\"${NC}"

    "${project_dir}/build/native/fabrics-perf" ${args}
  done
}
# Device to Host Intra-Host
function run_d2h_intrahost() {
  echo -e "\033[0;36mStarting Device-to-Host Intra-Host transfers\033[0m"
  methods=(Wait Spin)

  for format in "${formats[@]}"; do
    format_file="${project_dir}/config/flow-${format}.json"

    # libfabric
    for method in "${methods[@]}"; do
      test_name="MXLFabrics+Cuda2Host+SHM+OneWay+${method}"
      output_dir="${output_root}/d2h-intrahost/${format}/libfabric"
      args="--target ${runner_target_address} \
        --initiator ${runner_initiator_address} \
        --output ${output_dir} \
        --gpu ${gpu_id} \
        --run ${test_name} \
        --connect ${local_reflector_listener["libfabric"]} \
        --flow ${format_file} \
        --iterations ${nb_iter}"

      echo -e "${PURPLE}Running test \"${test_name}\" with image format \"${format}\" and output directory \"${output_dir}\" peer \"${local_reflector_listener["libfabric"]}\"${NC}"

      "${project_dir}/build/libfabric/fabrics-perf" ${args}
    done

    # NativeCuda
    test_name="NativeCuda+Cuda2Host"
    output_dir="${output_root}/d2h-intrahost/${format}/native"

    # addresses are dummy, no peer is required
    args="--target 127.0.0.1:10000 \
        --initiator 127.0.0.1:10001 \
        --output ${output_dir} \
        --gpu ${gpu_id} \
        --run ${test_name} \
        --connect 127.0.0.1:8080 \
        --flow ${format_file} \
        --iterations ${nb_iter}"

    echo "Running test \"${test_name}\" with image format \"${format}\" and output directory \"${output_dir}\""

    "${project_dir}/build/native/fabrics-perf" ${args}
  done
}

# Device to Device Intra-Host
# function run_d2d_intrahost() {
# }

## This where the main thread starts
trap 'echo "Aborting..."; exit 1' SIGINT

# run_d2d_interhost
# run_dh2hd_interhost
run_h2d_intrahost
# run_d2h_intrahost
