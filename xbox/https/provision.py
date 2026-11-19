"""Provision a single Xbox hardware test. Only upload server.pem and key.pem to the Xbox."""
import argparse
import ipaddress
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("ip", type=ipaddress.IPv4Address)
parser.add_argument("output", type=Path)
parser.add_argument("--openssl", default="openssl")
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=False)

with tempfile.TemporaryDirectory() as tmp:
    def openssl(*arguments):
        subprocess.run([args.openssl, *arguments], cwd=tmp, check=True)

    def new_key(path):
        openssl("genpkey", "-algorithm", "EC", "-pkeyopt", "ec_paramgen_curve:P-256", "-out", path)

    new_key("root.key")
    openssl("req", "-x509", "-new", "-key", "root.key", "-days", "30", "-sha256",
            "-subj", "/CN=Neon Roads Xbox hardware test CA", "-out", "root.pem",
            "-addext", "basicConstraints=critical,CA:TRUE,pathlen:0",
            "-addext", "keyUsage=critical,keyCertSign,cRLSign")
    new_key(output / "key.pem")
    openssl("req", "-new", "-key", output / "key.pem", "-subj", f"/CN={args.ip}", "-out", "server.csr")
    Path(tmp, "extensions").write_text(f"subjectAltName=IP:{args.ip}\n"
        "basicConstraints=critical,CA:FALSE\nextendedKeyUsage=serverAuth\nkeyUsage=critical,digitalSignature\n")
    openssl("x509", "-req", "-in", "server.csr", "-CA", "root.pem", "-CAkey", "root.key",
            "-CAcreateserial", "-days", "14", "-sha256", "-extfile", "extensions", "-out", output / "server.pem")
    openssl("x509", "-in", "root.pem", "-outform", "DER", "-out", output / "NeonRoads.cer")
    openssl("verify", "-CAfile", "root.pem", "-verify_ip", str(args.ip), output / "server.pem")
