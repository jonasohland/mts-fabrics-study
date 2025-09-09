#!/bin/bash
#!/bin/bash

# Default values
bind_addr="127.0.0.1"
native_target_port="9001"
native_initiator_port="9002"
native_listen_port="9091"
libfabric_target_port="8001"
libfabric_initiator_port="8002"
libfabric_listen_port="9090"
gpu_id="0"

usage() {
  cat <<EOF
Usage: $0 [OPTIONS]

Configure network addresses and GPU settings.

OPTIONS:
    -a  ADDR    Listen Address (default: ${bind_addr})
    -nl PORT    Native listen port (default: ${native_listen_port})
    -nt PORT    Native target port (default: ${native_target_port})
    -ni PORT    Native initiator port (default: ${native_initiator_port})
    -ll PORT    Libfabric listen port (default: ${libfabric_listen_port})
    -lt PORT    Libfabric target port (default: ${libfabric_target_port})
    -li PORT    Libfabric initiator port (default: ${libfabric_initiator_port})
    -g  ID      GPU ID (default: $gpu_id)
    -h          Show this help message

EXAMPLES:
    $0 -a 10.26.132.42 -nl 8080 -nt 7001 -ni 7002 -ll 8081 -lt 6001 -li 6002 -g 1
EOF
}

while [[ $# -gt 0 ]]; do
  case $1 in
  -a)
    bind_addr="$2"
    shift 2
    ;;
  -nl)
    native_listen_port="$2"
    shift 2
    ;;
  -nt)
    native_target_port="$2"
    shift 2
    ;;
  -ni)
    native_initiator_port="$2"
    shift 2
    ;;
  -ll)
    libfabric_listen_port="$2"
    shift 2
    ;;
  -lt)
    libfabric_target_port="$2"
    shift 2
    ;;
  -li)
    libfabric_initiator_port="$2"
    shift 2
    ;;
  -g)
    gpu_id="$OPTARG"
    shift 2
    ;;
  -h)
    usage
    exit 0
    ;;
  \?)
    echo "Invalid option: -$OPTARG" >&2
    usage >&2
    exit 1
    ;;
  :)
    echo "Option -$OPTARG requires an argument." >&2
    usage >&2
    exit 1
    ;;
  esac
done

echo "Bind Addr ${bind_addr}"

native_target="${bind_addr}:${native_target_port}"
native_initiator="${bind_addr}:${native_initiator_port}"
native_listen="${bind_addr}:${native_listen_port}"
libfabric_target="${bind_addr}:${libfabric_target_port}"
libfabric_initiator="${bind_addr}:${libfabric_initiator_port}"
libfabric_listen="${bind_addr}:${libfabric_listen_port}"

script_dir="$(cd -- "$(dirname -- "$(readlink -f "${BASH_SOURCE[0]}")")" &>/dev/null && pwd)"
project_dir="$(realpath "${script_dir}/../..")"

"${project_dir}/build/native/fabrics-perf" \
  --gpu "${gpu_id}" \
  --target "${native_target}" \
  --initiator "${native_initiator}" \
  --listen "${native_listen}" &
PID_NATIVE=$!

"${project_dir}/build/libfabric/fabrics-perf" \
  --gpu "${gpu_id}" \
  --target "${libfabric_target}" \
  --initiator "${libfabric_initiator}" \
  --listen "${libfabric_listen}" &
PID_LIBFABRIC=$!

trap "kill $PID_NATIVE $PID_LIBFABRIC 2>/dev/null" SIGINT

wait $PID_NATIVE $PID_LIBFABRIC

exit 0
