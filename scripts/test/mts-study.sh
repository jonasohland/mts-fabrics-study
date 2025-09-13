#! /bin/bash
script_dir="$(cd -- "$(dirname -- "$(readlink -f "${BASH_SOURCE[0]}")")" &>/dev/null && pwd)"
project_dir="$(realpath "${script_dir}/../..")"

PURPLE="\033[0;35m"
CYAN="\033[0;36m"
NC="\033[0m"

# Default addresses (without ports)
remote_reflector_address="10.26.132.43"
local_reflector_address="127.0.0.1"
runner_address="10.26.132.23"

# Default ports
declare -A remote_reflector_ports=(["libfabric"]="9090" ["native"]="9091")
declare -A local_reflector_ports=(["libfabric"]="9090" ["native"]="9091")
declare -A runner_ports=(["initiator"]="8001" ["target"]="8002")

gpu_id=("0")
nb_iter="200"
formats=(720 1080 2160)
functions_to_run=("d2d-interhost" "dh2hd-interhost" "h2d-intrahost" "d2h-intrahost" "d2d-intrahost")
output_root="${project_dir}/output"

# Function to display usage
usage() {
  cat <<EOF
Usage: $0 [OPTIONS]
Configure runner parameters and reflector addresses.

OPTIONS:
    -rra, --remote-reflector-address ADDR           Remote reflector address (default: $remote_reflector_address)
    -lra, --local-reflector-address ADDR            Local reflector address (default: $local_reflector_address)
    -ra, --runner-address ADDR                      Runner address (default: $runner_address)
    -rrpl, --remote-reflector-port-libfabric PORT   Remote reflector libfabric port (default: ${remote_reflector_ports["libfabric"]})
    -rrpn, --remote-reflector-port-native PORT      Remote reflector native port (default: ${remote_reflector_ports["native"]})
    -lrpl, --local-reflector-port-libfabric PORT    Local reflector libfabric port (default: ${local_reflector_ports["libfabric"]})
    -lrpn, --local-reflector-port-native PORT       Local reflector native port (default: ${local_reflector_ports["native"]})
    -rpi, --runner-port-initiator PORT              Runner initiator port (default: ${runner_ports["initiator"]})
    -rpt, --runner-port-target PORT                 Runner target port (default: ${runner_ports["target"]})
    -r, --run FUNCTIONS                             Functions to run (comma-separated", default: $(
    IFS=,
    echo "${functions_to_run[*]}"
  ))

    -g, --gpu LIST                                  GPU IDs (comma-separated, default: $(
    IFS=,
    echo "${gpu_id[*]}"
  ))
    -n, --iterations NUM                            Number of iterations (default: $nb_iter)
    -f, --formats LIST                              Formats (comma-separated, default: $(
    IFS=,
    echo "${formats[*]}"
  ))
    -o --output                                     Output directory (default: $output_root)
    -h, --help                                      Show this help message
EOF
}

# Function to parse comma-separated list into array
parse_list() {
  local input="$1"
  local -n output_array=$2
  IFS=',' read -ra output_array <<<"$input"
}

# Parse command line options
while [[ $# -gt 0 ]]; do
  case $1 in
  -rra | --remote-reflector-address)
    remote_reflector_address="$2"
    shift 2
    ;;
  -lra | --local-reflector-address)
    local_reflector_address="$2"
    shift 2
    ;;
  -ra | --runner-address)
    runner_address="$2"
    shift 2
    ;;
  -rrpl | --remote-reflector-port-libfabric)
    remote_reflector_ports["libfabric"]="$2"
    shift 2
    ;;
  -rrpn | --remote-reflector-port-native)
    remote_reflector_ports["native"]="$2"
    shift 2
    ;;
  -lrpl | --local-reflector-port-libfabric)
    local_reflector_ports["libfabric"]="$2"
    shift 2
    ;;
  -lrpn | --local-reflector-port-native)
    local_reflector_ports["native"]="$2"
    shift 2
    ;;
  -rpi | --runner-port-initiator)
    runner_ports["initiator"]="$2"
    shift 2
    ;;
  -rpt | --runner-port-target)
    runner_ports["target"]="$2"
    shift 2
    ;;
  -r | --run)
    parse_list "$2" functions_to_run
    shift 2
    ;;
  -g | --gpu)
    parse_list "$2" gpu_id
    shift 2
    ;;
  -n | --iterations)
    nb_iter="$2"
    shift 2
    ;;
  -f | --formats)
    parse_list "$2" formats
    shift 2
    ;;
  -o | --output)
    output_root="$2"
    shift 2
    ;;
  -h | --help)
    usage
    exit 0
    ;;
  *)
    echo "Unknown option: $1" >&2
    usage >&2
    exit 1
    ;;
  esac
done

declare -A remote_reflector_listener=(
  ["libfabric"]="${remote_reflector_address}:${remote_reflector_ports["libfabric"]}"
  ["native"]="${remote_reflector_address}:${remote_reflector_ports["native"]}"
)

declare -A local_reflector_listener=(
  ["libfabric"]="${local_reflector_address}:${local_reflector_ports["libfabric"]}"
  ["native"]="${local_reflector_address}:${local_reflector_ports["native"]}"
)

runner_initiator_address="${runner_address}:${runner_ports["initiator"]}"
runner_target_address="${runner_address}:${runner_ports["target"]}"

display_array() {
  local -n arr=$1
  local name="$2"
  echo "  $name:"
  for element in "${arr[@]}"; do
    echo "    - $element"
  done
}

display_assoc_array() {
  local -n arr=$1
  local name="$2"
  echo "  $name:"
  for key in "${!arr[@]}"; do
    echo "    $key: ${arr[$key]}"
  done
}

# Display current configuration
echo -e "${CYAN}Configuration:${NC}"
display_assoc_array remote_reflector_listener "Remote reflector listeners"
display_assoc_array local_reflector_listener "Local reflector listeners"
echo "  Runner initiator address:   $runner_initiator_address"
display_array gpu_id "GPU IDs"
echo "  Number of iterations:       $nb_iter"
display_array formats "Formats"
echo "  Output Directory:           $output_root"
echo ""

# Device to Device Inter-Hos
function run_d2d_interhost() {
  echo -e "${CYAN}Starting Device-to-Device Inter-Host transfers${NC}"

  methods=(Wait Spin)
  tests=(Cuda2Cuda)
  libraries=("native" "libfabric")

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
          --gpu ${gpu_id[0]} \
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
  echo -e "${CYAN}Starting Host-to-Device-to-Host-to-Device Inter-Host transfers${NC}"

  methods=(Wait Spin)
  tests=(Cuda2Cuda Cuda2Host2Host2Cuda)
  libraries=("native" "libfabric")

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
          --gpu ${gpu_id[0]} \
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
        --gpu ${gpu_id[0]} \
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
        --gpu ${gpu_id[0]} \
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
  echo -e "${CYAN}Starting Device-to-Host Intra-Host transfers${NC}"
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
        --gpu ${gpu_id[0]} \
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
        --gpu ${gpu_id[0]} \
        --run ${test_name} \
        --connect 127.0.0.1:8080 \
        --flow ${format_file} \
        --iterations ${nb_iter}"

    echo -e "${PURPLE}Running test \"${test_name}\" with image format \"${format}\" and output directory \"${output_dir}\"${NC}"

    "${project_dir}/build/native/fabrics-perf" ${args}
  done
}

# Device to Device Intra-Host
function run_d2d_intrahost() {
  echo -e "${CYAN}Starting Device-to-Device Intra-Host transfers${NC}"
  methods=(Wait Spin)

  for format in "${formats[@]}"; do
    format_file="${project_dir}/config/flow-${format}.json"

    # libfabric
    for method in "${methods[@]}"; do
      test_name="MXLFabrics+Cuda2Cuda+SHM+OneWay+${method}"
      output_dir="${output_root}/d2d-intrahost/${format}/libfabric"
      args="--target ${runner_target_address} \
        --initiator ${runner_initiator_address} \
        --output ${output_dir} \
        --gpu ${gpu_id[0]} ${gpu_id[1]} \
        --run ${test_name} \
        --connect ${local_reflector_listener["libfabric"]} \
        --flow ${format_file} \
        --iterations ${nb_iter}"

      echo -e "${PURPLE}Running test \"${test_name}\" with image format \"${format}\" and output directory \"${output_dir}\" peer \"${local_reflector_listener["libfabric"]}\"${NC}"

      "${project_dir}/build/libfabric/fabrics-perf" ${args}
    done

    # NativeCuda
    test_name="NativeCuda+Cuda2Cuda"
    output_dir="${output_root}/d2d-intrahost/${format}/native"

    # addresses are dummy, no peer is required
    args="--target 127.0.0.1:10000 \
        --initiator 127.0.0.1:10001 \
        --output ${output_dir} \
        --gpu ${gpu_id[0]} ${gpu_id[1]} \
        --run ${test_name} \
        --connect 127.0.0.1:8080 \
        --flow ${format_file} \
        --iterations ${nb_iter}"

    echo -e "${PURPLE}Running test \"${test_name}\" with image format \"${format}\" and output directory \"${output_dir}\"${NC} first gpu id \"${gpu_id[0]}\" second gpu id \"${gpu_id[1]}\""

    "${project_dir}/build/native/fabrics-perf" ${args}
  done

}

trap 'echo "Aborting..."; exit 1' SIGINT

transform_to_function_name() {
  local arg="$1"
  echo "run_${arg//-/_}"
}

for arg in "${functions_to_run[@]}"; do
  # Validate argument name
  func=$(transform_to_function_name "${arg}")
  if declare -f "${func}" >/dev/null; then
    "$func"
  else
    echo "Warning: Function '$func' is not defined"
  fi
done
