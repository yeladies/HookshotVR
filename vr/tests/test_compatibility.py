"""Windows guard tests using authored NSO-like headers and dummy files, never a real installation."""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
@unittest.skipUnless(shutil.which("powershell"),"Windows PowerShell guard fixture")
class CompatibilityTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix="vr-compat-");self.root=Path(self.temp.name)
        for p in ("vr","gpu/vk","install/.hookshot_exefs"): (self.root/p).mkdir(parents=True,exist_ok=True)
        self.build="01"*20
        (self.root/"vr/upstream.json").write_text(json.dumps({"schema":1,"mod_host_renderer_abi":18,"supported_game_builds":[self.build]}))
        (self.root/"gpu/vk/vk_api.h").write_text("#define MKWVK_API_VERSION 18\n")
        data=bytearray(96);data[:4]=b"NSO0";data[64:84]=bytes.fromhex(self.build)
        (self.root/"install/.hookshot_exefs/main").write_bytes(data)
        (self.root/"renderer.dll").write_bytes(b"authored fake renderer")
        (self.root/"host.exe").write_bytes(b"authored fake host")
    def tearDown(self):self.temp.cleanup()
    def run_ps(self,body,success=True):
        quote=lambda p:str(p).replace("'","''")
        script="$ErrorActionPreference='Stop'; . '"+quote(ROOT/"vr/compatibility.ps1")+"'; $root='"+quote(self.root)+"'; "+body
        result=subprocess.run(["powershell","-NoProfile","-Command",script],capture_output=True,text=True)
        self.assertEqual(result.returncode==0,success,result.stdout+result.stderr)
    def test_build_id_guard(self):
        self.run_ps("$null=Assert-VrSupportedInstallation $root (Join-Path $root 'install')")
        before=(self.root/"install/.hookshot_exefs/main").read_bytes()
        (self.root/"vr/upstream.json").write_text(json.dumps({"schema":1,"mod_host_renderer_abi":18,"supported_game_builds":["02"*20]}))
        self.run_ps("$null=Assert-VrSupportedInstallation $root (Join-Path $root 'install')",False)
        self.assertEqual((self.root/"install/.hookshot_exefs/main").read_bytes(),before)
    def test_abi_guard(self):
        (self.root/"gpu/vk/vk_api.h").write_text("#define MKWVK_API_VERSION 19\n")
        self.run_ps("$null=Get-VrManifest $root",False)
    def test_receipt_and_hash_guard(self):
        prefix="Write-VrBuildReceipt $root (Join-Path $root 'receipt.json') (Join-Path $root 'renderer.dll') (Join-Path $root 'host.exe') 'source1' $true; "
        check="Assert-VrBuildReceipt $root (Join-Path $root 'receipt.json') (Join-Path $root 'renderer.dll') (Join-Path $root 'host.exe') 'source1' $true"
        self.run_ps(prefix+check)
        (self.root/"renderer.dll").write_bytes(b"changed renderer")
        self.run_ps(check,False)
    def test_stale_source_guard(self):
        self.run_ps("Write-VrBuildReceipt $root (Join-Path $root 'receipt.json') (Join-Path $root 'renderer.dll') '' 'old' $true; Assert-VrBuildReceipt $root (Join-Path $root 'receipt.json') (Join-Path $root 'renderer.dll') '' 'new' $true",False)
if __name__=="__main__":unittest.main()