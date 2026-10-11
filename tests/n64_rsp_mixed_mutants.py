#!/usr/bin/env python3
"""Execute each mixed-product signedness regression against corrected observations."""
import argparse, hashlib, json, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
FAULTS={
 "vmudm-signed-vt":("case 5:value=a*int64_t(right[i]);","case 5:value=int64_t(left[i])*b;"),
 "vmudn-signed-vs":("case 6:value=int64_t(left[i])*b;","case 6:value=a*int64_t(right[i]);"),
 "vmadm-signed-vt":("case 13:value=old+a*int64_t(right[i]);","case 13:value=old+int64_t(left[i])*b;"),
 "vmadn-signed-vs":("case 14:value=old+int64_t(left[i])*b;","case 14:value=old+a*int64_t(right[i]);"),
}
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--compiler",default="c++");parser.add_argument("--artifacts",type=Path,default=ROOT/"build/artifacts/component/n64_rsp_mixed");args=parser.parse_args()
 source=ROOT/"cores/n64/src";files={p.name:p.read_bytes() for p in sorted(source.glob("*.hpp"))};files.update({p.name:p.read_bytes() for p in (source/"machine.cpp",source/"vr4300.cpp")})
 reports=[]
 for name in ("baseline",*FAULTS):
  changed=dict(files)
  if name!="baseline":
   old,new=FAULTS[name];body=changed["rsp_vector.hpp"].decode();assert body.count(old)==1,name;changed["rsp_vector.hpp"]=body.replace(old,new).encode()
  directory=(args.artifacts/name).resolve();directory.mkdir(parents=True,exist_ok=True)
  for filename,content in changed.items():(directory/filename).write_bytes(content)
  binary=directory/"test";compiled=subprocess.run([args.compiler,"-std=c++17","-O2","-Wall","-Wextra","-Werror","-I"+str(directory),"-I"+str(ROOT/"include"),str(ROOT/"tests/cpp/n64_rsp_multiply_test.cpp"),str(directory/"machine.cpp"),str(directory/"vr4300.cpp"),"-o",str(binary)],capture_output=True,text=True,timeout=90)
  (directory/"compile.log").write_text(compiled.stdout+compiled.stderr)
  if compiled.returncode:raise RuntimeError("Compilation is not a mutation rejection: "+name)
  result=subprocess.run([str(binary),str(ROOT/"tests/roms-src/n64/rsp_multiply_pairs/rsp_multiply_pairs.z64")],capture_output=True,text=True,timeout=90);(directory/"execution.log").write_text(result.stdout+result.stderr)
  if name=="baseline":assert result.returncode==0 and result.stdout.endswith("passed\n"),name
  else:assert result.returncode==1 and result.stderr.startswith("hardware observation fn="),name
  reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
 report=dict(status="pass",production_source_sha256=hashlib.sha256(b"".join(files.values())).hexdigest(),test_sha256=hashlib.sha256((ROOT/"tests/cpp/n64_rsp_multiply_test.cpp").read_bytes()).hexdigest(),results=reports);(args.artifacts/"report.json").write_text(json.dumps(report,indent=2)+"\n");print("N64 mixed RSP products: baseline passes; four executed signedness faults rejected")
if __name__=="__main__":main()
