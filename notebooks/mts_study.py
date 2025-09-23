import polars as pl
from enum import Enum
from dataclasses import dataclass
from pathlib import Path

schema = pl.Schema(
    {"Timers": pl.UInt64(), "TxTime": pl.UInt64(), "RxTime": pl.UInt64()}
)


class Library(Enum):
    libfabric = "libfabric"
    libfabric_fix = "libfabric-fix"
    native = "native"
    ucx = "ucx"


class Format(Enum):
    _720 = "720"
    _1080 = "1080"
    _2160 = "2160"


class Completion(Enum):
    spin = "Spin"
    wait = "Wait"
    none = ""


class Transport(Enum):
    tcp = "Tcp"
    verbs = "Verbs"
    shm = "SHM"
    none = ""


class TransferMode(Enum):
    oneway = "OneWay"
    reflect = "Reflect"
    none = ""


class Movement(Enum):
    d2d = "Cuda2Cuda"
    dh2hd = "Cuda2Host2Host2Cuda"
    h2d = "Host2Cuda"
    d2h = "Cuda2Host"

    def to_study_name(self) -> str:
        match self:
            case Movement.d2d:
                return "Device-to-Device"
            case Movement.dh2hd:
                return "Device-to-Host-to-Host-to-Device"
            case Movement.h2d:
                return "Host-to-Device"
            case Movement.d2h:
                return "Device-to-Host"


class Test(Enum):
    mxl_fabrics = "MXLFabrics"
    native_cuda = "NativeCuda"
    ucx = "UCX"


@dataclass(frozen=True, eq=True)
class TestConfiguration:
    library: Library
    format: Format
    completion: Completion
    test: Test
    movement: Movement
    transport: Transport
    tx_mode: TransferMode


def get_mxlfabrics_test_name(conf: TestConfiguration):
    return f"{conf.test.value}+{conf.movement.value}+{conf.transport.value}+{conf.tx_mode.value}+{conf.completion.value}"


def get_nativecuda_test_name(conf: TestConfiguration):
    return f"{conf.test.value}+{conf.movement.value}"


def get_ucx_test_name(conf: TestConfiguration):
    # UCX+Cuda2Cuda+Reflect+Wait
    return f"{conf.test.value}+{conf.movement.value}+{conf.tx_mode.value}+{conf.completion.value}"


def diff_loader(data):
    return data["RxTime"] - data["TxTime"]


def timers_loader(data):
    return data["Timers"]


def load_data(
    directory,
    loader,
    libraries=Library,
    formats=Format,
    completions=Completion,
    tests=Test,
    movements=Movement,
    transports=Transport,
    tx_modes=TransferMode,
):
    data = {}
    perf = {}
    pcie = {}
    for format in formats:
        for tx_mode in tx_modes:
            for test in tests:
                if test == Test.mxl_fabrics:
                    for comp in completions:
                        for transport in transports:
                            for library in libraries:
                                for movement in movements:
                                    test_conf = TestConfiguration(
                                        library,
                                        format,
                                        comp,
                                        test,
                                        movement,
                                        transport,
                                        tx_mode,
                                    )
                                    file_name = f"{directory}/{format.value}/{library.value}/{get_mxlfabrics_test_name(test_conf)}"
                                    data[test_conf] = loader(
                                        pl.read_csv(f"{file_name}.csv", schema=schema)
                                    )
                                    perf[test_conf] = pl.read_csv(
                                        f"{file_name}.perf.csv"
                                    )
                                    if Path(f"{file_name}.pcm.pcie.csv").exists():
                                        pcie[test_conf] = pl.read_csv(
                                            f"{file_name}.pcm.pcie.csv"
                                        )
                elif test == Test.native_cuda:
                    library = Library.native
                    test_conf = TestConfiguration(
                        library,
                        format,
                        Completion.none,
                        test,
                        movement,
                        Transport.none,
                        TransferMode.none,
                    )
                    file_name = f"{directory}/{format.value}/{library.value}/{get_nativecuda_test_name(test_conf)}"
                    data[test_conf] = timers_loader(
                        pl.read_csv(f"{file_name}.csv", schema=schema)
                    )
                    perf[test_conf] = pl.read_csv(f"{file_name}.perf.csv")
                    if Path(f"{file_name}.pcm.pcie.csv").exists():
                        pcie[test_conf] = pl.read_csv(f"{file_name}.pcm.pcie.csv")
                elif test == Test.ucx:
                    for movement in movements:
                        for comp in completions:
                            test_conf = TestConfiguration(
                                Library.ucx,
                                format,
                                comp,
                                test,
                                movement,
                                Transport.none,
                                tx_mode,
                            )
                            file_name = f"{directory}/{format.value}/ucx/{get_ucx_test_name(test_conf)}"
                            data[test_conf] = loader(
                                pl.read_csv(f"{file_name}.csv", schema=schema)
                            )
                            perf[test_conf] = pl.read_csv(f"{file_name}.perf.csv")
                            if Path(f"{file_name}.pcm.pcie.csv").exists():
                                pcie[test_conf] = pl.read_csv(
                                    f"{file_name}.pcm.pcie.csv"
                                )

    return data, perf, pcie


def make_table(data, perf_data, pcie_data=None, columns=[]):
    df = pl.DataFrame()
    for test_conf, values in data.items():
        table_data = {}
        perf = perf_data[test_conf]

        for col in columns:
            table_data[col] = getattr(test_conf, col)
        table_data["lat_mean"] = round(values.mean() / 1e6, 4)
        table_data["lat_max"] = round(values.max() / 1e6, 4)
        table_data["lat_std"] = round(values.std() / 1e6, 4)
        table_data["cpu_usage"] = round(
            (perf["task_clock_user"][0] + perf["task_clock_kernel"][0])
            / perf["time_elapsed"][0],
            4,
        )
        if pcie_data is not None:
            table_data["pcie_bw"] = round(
                pcie_data[test_conf]["Total PCIE Bandwidth (GB/s)"].mean(), 4
            )

        df = df.vstack(pl.DataFrame(table_data))
    return df
