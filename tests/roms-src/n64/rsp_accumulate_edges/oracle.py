from build import base
import importlib.util,sys
from pathlib import Path
# Load the independent oracle with its input helper module bound explicitly.
previous=sys.modules.get('build');sys.modules['build']=base
spec=importlib.util.spec_from_file_location('multiply_oracle',Path(__file__).parent.parent/'rsp_multiply_pairs/oracle.py')
oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
if previous is None:del sys.modules['build']
else:sys.modules['build']=previous
def expected(case):return oracle.expected(case,True)
