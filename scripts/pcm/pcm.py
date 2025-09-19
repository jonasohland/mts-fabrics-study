from flask import Flask, request
import subprocess
import polars as pl

# Run with the following command
# flask --app pcm run --host=0.0.0.0

app = Flask("pcm")


def parse_amduprofpcm(lines):
    print(lines)
    samples = []
    header = []
    found_header = False
    for line in lines:
        if found_header and line != "\n":
            sample = {}
            values = line.split(",")
            for metric, value in zip(header, values):
                if value != "" and value != "\n":
                    sample[metric] = value
            samples.append(sample)

        if line.startswith("Total"):
            header = [
                metric for metric in line.split(",") if metric != "\n" and metric != ""
            ]
            found_header = True
    df = pl.DataFrame(samples)

    return df.write_csv()


@app.route("/pcie")
def pcie():
    nb_iter = request.args.get("nb_iter", default=200, type=int)
    fps = request.args.get("fps", default=30, type=int)
    capture_time = nb_iter // fps // 2

    print(f"Sampling for {capture_time} seconds")
    output_file = "/tmp/pcm.csv"
    subprocess.run(
        [
            "AMDuProfPcm",
            "--msr",
            "-m",
            "pcie",
            "-d",
            str(capture_time),
            "-o",
            output_file,
        ]
    )

    with open(output_file) as f:
        return parse_amduprofpcm(f.readlines())

    return None


@app.route("/mem")
def mem():
    nb_iter = request.args.get("nb_iter", default=200, type=int)
    fps = request.args.get("fps", default=30, type=int)
    capture_time = nb_iter // fps // 2

    print(f"Sampling for {capture_time} seconds")

    output_file = "/tmp/mem.csv"
    subprocess.run(
        [
            "AMDuProfPcm",
            "--msr",
            "-m",
            "memory",
            "-d",
            str(capture_time),
            "-o",
            output_file,
        ]
    )

    with open(output_file) as f:
        return parse_amduprofpcm(f.readlines())

    return None
