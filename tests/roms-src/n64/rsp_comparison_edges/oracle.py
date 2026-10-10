from pathlib import Path
import importlib.util
spec=importlib.util.spec_from_file_location("comparison_oracle",Path(__file__).parent.parent/"rsp_comparison_pairs/oracle.py")
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
def expected(case):return module.expected(case,True)
