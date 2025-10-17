# mts-fabrics-study

Test Harness as presented at SMPTE MTS 2025 used to compare the performance of `libfabric`, `UCX` and `libibverbs`.

## Usage

### Building the binaries

To be able to run the tests correctly for `libfabric`, `UCX` and `native RDMA` you will need to generate 2 different binaries of `fabrics-perf`.

#### Generate native RDMA

```sh
# assumes you are running from the root directory
$ mkdir -p build && cd build
$ cmake --preset Linux-Clang-Release -B native -DMXL_FABRICS_NATIVE=1 ..
$ cmake --build native
```

#### Generate libfabric

```sh
# assumes you are running from the <root_dir>/build directory
$ cmake --preset Linux-Clang-Release -B libfabric -DMXL_FABRICS_OFI=1 ..
$ cmake --build libfabric
```

#### UCX

UCX implementation is part of the `fabrics-perf` binary so it is already bundled in both `native` and `libfabric` version of `fabrics-perf`.

### Running a single test case

```sh
# assuming we want to run tests using libfabric from root_dir
$ ./build/libfabric/fabrics-perf -t <if_addr_1>:9001 -i <if_addr_1>:9002 -l <if_addr_1>:9090 # starting the reflector
$ ./build/libfabric/fabrics-perf -t <if_addr_2>:8001 -i <if_addr_2>:8002 -c <if_addr_2>:9090 -f config/flow-1080.json -r MXLFabrics+Host2Host+Verbs+Reflect+Wait # starting the runner
```

`if_addr_1` corresponds to the ip address of the interface you want to use on the first node
`if_addr_2` corresponds to the ip address of the interface you want to use on second node

If the configuration is correct, this will run 2000 iterations of a ping-pong RDMA test using host memory on both side. If omit the `-r` option on the runner, you will see all the available test case to run.

By default, the domain used is `/dev/shm/mxl` but you can change it with the `-d` switch

### Running a suite of test cases

This allows to run a suite of test cases that corresponds to a specific data movement. There is a bash script that already have routines to test cases to run for a specific data movement we've studied.

```sh
./scripts/test/mts-study-reflector.sh -a <peer_ip_addr> # Must be started first on the remote peer for internode tests! (you can check the other options to change the default ports it uses)
./scripts/test/mts-study-reflector.sh # Started where the runner will be running for intranode tests
./scripts/test/mts-study.sh # This runs the actual test cases (the runner)
```

\*\*\* All these tests requires at least one GPU and only Nvidia GPUs are supported.

By default `mts-study.sh` will run all the data movement suite of test cases. You can control which one to run with the `-r` switch.

- d2d-interhost = GPUDirect RDMA
- dh2hd-interhost = Device to Device Internode without using GPUDirect RDMA
- h2d-intrahost = Host to Device Intranode
- d2h-intrahost = Device to Host Intranode
- d2d-intrahost = Device to Device Intranode

When running `d2d-intrahost` test, you will need to have at least 2 GPU and specify both GPU index with the `-g` switch

By default, the data will output in `<root_dir>/output`, but if you want to visualize new data with the notebook, you will have to output in the data folder using `-o data/mts-study`

Putting all together..

```sh
./scripts/test/mts-study.sh -o data/mts-study -g 0 1 -ra <runner_ip_addr> -rra <reflector_ip_addr>
```

### Visualize the tests results

Install [jupyter-lab](ttps://jupyter.org/) and launch an instance.

The notebook can found under `notebooks/mts-study.ipynb`.

Simply re-run all the cells to see the data update.
